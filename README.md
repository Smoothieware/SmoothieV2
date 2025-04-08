# smoothie version 2 for STM32H745 (and STM32H743)
Smoothie V2 using STM32 HAL and FreeRTOS.

All essential modules are ported and seem to work.

NOTE on STM32H745 the M4 Core has to be configured to be asleep on boot
(see the Initial Bootstrap section in Firmware/README.md).
There is currently no code to support the M4 Core, everything runs on the M7 core.
There is a considerable amount of work to be done to support the dual core operation.


The preferred and supported development environment is Linux.

Currently runs on the NUCLEO-H745ZI-Q board (no longer supported though), a Devebox 743 board and of course a Smoothie Prime board.

Currently uses the following toolchain..

    gcc version 10.3.1 20210621 (release) (15:10.3-2021.07-4)

*NOTE* if you use a different version you are likely to get compile errors
 which you will have to figure out as currently the only supported compiler
 is 10.3.1 as it has been tested for a long time and known to work with the
 code.
    
To get the tool chain you should do the following on Ubuntu based Linuxes...

	wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2?rev=78196d3461ba4c9089a67b5f33edf82a&hash=D484B37FF37D6FC3597EBE2877FB666A41D5253B

or for older versions (but make sure it is 10.3.1)

    sudo apt-get install gcc-arm-none-eabi

or for Debian Stretch (and Ubuntu) get the tar from here...

    https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads

Then detar to a directory and do...

        export ARMTOOLS=/downloaddir/gcc-arm-none-eabi-{version}/bin
        (replacing {version} with the version you downloaded)

You need to install ruby (and rake) to build...

    > sudo apt-get install ruby 

To build first cd to the Firmware directory, then...
    
    rake -m

To build all unit tests 
    
    rake testing=1 -m

To compile only some unit tests in Firmware:

    rake testing=1 test=streams
    rake testing=1 test=dispatch,streams,planner

To compile with debug symbols...

    rake testing=1 test=streams debug=1 

To compile a unit test that tests a module, and to include that module

    rake testing=1 test=temperatureswitch modules=tools/temperatureswitch   

As a convenience if dfu-util is setup you can do this to flash the build...
    
    rake target=Prime flash

## Windows
The rake build system will run on Windows, however some utilities need to be installed first.

1. Install git - https://github.com/git-for-windows/git/releases/download/v2.31.1.windows.1/Git-2.31.1-64-bit.exe (or current version)
2. Install ruby - https://rubyinstaller.org/downloads/
3. (Optionally) Install python3 - https://www.python.org/ftp/python/3.9.5/python-3.9.5-amd64.exe (or current v3 version)
4. Install gcc-arm-none-eabi - https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-win32.zip?revision=ffcaa06c-940d-4319-8d7e-d3070092d392&la=en&hash=130196DDF95B0D9817E1FDE08A725C7EBFBFB5B8 unzip the file and move to ```/usr``` or set the ```ARMTOOLS``` environment variable to where the bin directory is.

make sure they are all added to the PATH, then in powershell navigate to the Firmware folder and type ```rake -m```

See https://github.com/Smoothieware/SmoothieV2/blob/master/Firmware/README.md for flashing details.

