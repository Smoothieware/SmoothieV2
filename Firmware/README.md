Currently runs on the Prime Smoothoieboard, NUCLEO-H745ZI-Q board and Devebox 743 board.

To build the test cases do ```rake target=Prime testing=1 -m```
then flash to Prime, the results print out to the uart/serial found on the DEBUG Uart port.

To make the Firmware do ```rake target=Prime -m```

The config file is called config.ini on the sdcard and examples are shown in the ConfigSamples directory, config-3d.ini is for a 3d printer, and config-laser.ini is for laser, these would be renamed config.ini and copied to the sdcard.

Currently the max stepping rate is limited to 200Khz as this seems the upper limit to handle the step interrupt.

Enough modules have been ported to run a 3D printer, a CNC Router and also a laser is supported.

Modules that have been ported so far...

* endstops
* extruder
* laser
* switch
* temperaturecontrol
* temperatureswitch
* zprobe
* currentcontrol
* killbutton
* player
* network


On the Prime there are 4 leds..

1. led1 - smoothie led, flashes slowly when idle, does not flash when busy
2. led2 - smoothie led, on when executing moves, flashes when in HALT

leds3 and 4 indicate some sort of error as below.

led1 and led2 flash if there was a serious config error.
on,off,on,on means there was a stack overflow
all on means there was a Hardfault detected
(TBD All 4 leds flash if there is no sdcard inserted)

The debug UART port is on the STLink3 ACM0 on the Nucleo, and on the DEBUG Uart header on the Prime
baud rate 115200.

Initial Bootstrap
-----------------
V2 does not have a bootloader, the firmware itself can flash itself and do updates etc.
To bootstrap the inital firmware (or to recover from bad firmware or bricking) you can use a jlink to flash the firmware or use the STLINKV3 drag and drop, or use the BOOT0 USB flashing (press BOOT0 button and reset)...

Download STM32_Programmer_CLI from https://www.st.com/en/development-tools/stm32cubeprog.html

    STM32_Programmer_CLI -q -c port=usb1 -w smoothiev2_Nucleo/smoothiev2.bin 0x08000000 -rst
    (you can also use dfu-util -a 0 -D smoothiev2.bin --dfuse-address 0x08000000)

If using the STM32H745 you must first set the option bytes to only boot the M7 core... (not needed for STM32H743)
    
    STM32_Programmer_CLI -c port=swd -ob BCM4=0
    (use port=usb1 if using BOOT0 mode)

To unbrick if bad option bytes have been set...

    STM32_Programmer_CLI -c port=swd -rdu

or to see all option bytes...

    STM32_Programmer_CLI -c port=swd -ob displ

JLink has a better tool which fixes everything back to factory default...

    JLinkSTM32 -device STM32H745ZI_M7 -if SWD

Subsequent Updates
------------------
If the firmware is running then just copy the firmware bin to the sdcard as flashme.bin and reboot. You can use the ```update``` command if you have a network connected. There are also various ways to get the firmware onto the sdcard over the serial ports, name it flashme.bin and then use the ```flash``` command.

Debugging and Flashing
----------------------
Use a JLink to debug...

    /opt/jlink/JLinkGDBServer -device STM32H745ZI_M7 -if SWD -halt -speed 4000  -timeout 10000

Then run gdb:

    arm-none-eabi-gdb-64bit -ex "target remote localhost:2331" smoothiev2_Prime/smoothiev2.elf

The ```arm-none-eabi-gdb-64bit``` binary is in the tools directory, it is a fixed version that handles Hard Faults correctly. You can also use the arm-none tools if you have it installed.

To flash use the load command in gdb, it is recommended you do a ```mon reset``` before and after the load.

Once flashed you use the c command to run.

There is an optional module that can be loaded into qspi flash that will allow flashing via dfu-util, see 
DFUBootloader/README.md for more details.


Flashing V2 Smoothie using J-Link (Windows)
===========================================

Install and launch Jflash Lite
Set to STM32H745ZI_M7, JTAG 4000khz
Select V2 smoothie bin file
Set Address to 0x08000000
Click Program Device
Install SD with config.ini into V2 board
Plug into USB and boot.  Verify serial connection

Contributions
=============

Pull requests to the master branch will not be accepted as this will be the stable, well tested branch.
Pull requests should be made to the unstable branch, which is a branch off the edge branch.
Once a PR has been merged into unstable and has been fairly well tested it will be merged to the edge branch.
Only when edge is considered stable will it be merged into master.

