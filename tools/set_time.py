#!/usr/bin/python3
# set the smoothieV2 time
import time
import serial

ser = serial.Serial("/dev/ttyACM0", 115200, timeout=5)
ser.flushInput()  # Flush startup text in serial input

gotok = False
while not gotok:
    ser.write(b'\n')
    rep = ser.read_until()
    s = rep.decode(encoding='latin1', errors='ignore')
    gotok = s.startswith('ok')

ser.flushInput()  # Flush startup text in serial input

t = time.gmtime()
dt = "{:02}{:02}{:02}{:02}{:02}{:02}".format(t.tm_year - 2000, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec)
ser.write(bytes('date {}\n'.format(dt).encode('latin1')))

ser.close()
