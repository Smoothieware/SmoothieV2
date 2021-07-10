#!/usr/bin/python3
import serial
import sys
import subprocess
import signal
import traceback
import os
import argparse
import serial
import time


def signal_term_handler(signal, frame):
    global intrflg
    print('got SIGTERM...')
    sys.quit()


signal.signal(signal.SIGTERM, signal_term_handler)

# Define command line argument interface
parser = argparse.ArgumentParser(description='stream download file to smoothieV2 over USB serial')
parser.add_argument('file', help='filename to be downloaded')
parser.add_argument('device', help='Smoothie Serial Device')
parser.add_argument('-v', '--verbose', action='store_true', default=False, help='verbose output')
parser.add_argument('-f', '--flash', action='store_true', default=False, help='flash')
parser.add_argument('-o', '--outfn', nargs='?', default="", help='output file name')
args = parser.parse_args()

filename = args.file
dev = "/dev/tty{}".format(args.device)
if args.flash:
    outfile = "flashme.bin"
elif args.outfn:
    outfile = args.outfn
else:
    outfile = os.path.basename(filename)

if args.verbose:
    pass

f = open(filename, "rb")

filesize = os.path.getsize(filename)

print("Downloading file: {}, size: {} to {}".format(filename, filesize, dev))

ser = serial.Serial(dev, 115200, timeout=5)
time.sleep(1)
ser.flushInput()  # Flush startup text in serial input

gotok = False
while not gotok:
    ser.write(b'\n')
    rep = ser.read_until()
    if not rep:
        print("Timed out waiting for initial response")
        f.close()
        ser.close()
        exit(1)

    s = rep.decode(encoding='latin1', errors='ignore')
    gotok = s.startswith('ok')

ser.flushInput()  # Flush startup text in serial input

ser.write(bytes('dl {} {}\n'.format(outfile, filesize).encode('latin1')))

# wait for READY or FAIL
ll = ser.read_until().decode('latin1')
if ll.startswith('READY'):
    while True:
        # stream file
        buf = f.read(1024)
        if buf:
            ser.write(buf)
        else:
            ok = True
            print("Uploading done")
            break

        if ser.inWaiting():
            resp = ser.readline().decode('latin1')
            print("Got error: {}".format(resp))
            ok = False
            break

else:
    print("Got unexpected waiting for ready {}".format(ll))
    ok = False

if ok:
    # read response
    resp = ser.read_until()
    if resp:
        resp = resp.decode('latin1')
        if resp.startswith("SUCCESS"):
            ok = True
            print("Success")
        else:
            ok = False
            print("Got fail: {}".format(resp))
    else:
        print("Timed out waiting for response")
        ok = False

if ok and args.flash:
    ser.write(b'flash\n')
    resp = ser.read_until().decode('latin1')
    if resp:
        print(resp)
    else:
        print("No response from the flash command")

f.close()
ser.close()
