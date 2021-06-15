# smoothie version 2 for STM32H745 (and STM32H743)
Smoothie V2 using STM32 HAL and FreeRTOS.

The preferred and supported development environment is Linux.

Currently runs on the NUCLEO-H745ZI-Q board and Devebox 743 board

Currently uses the following toolchain..
    gcc version 9.2.1 20191025 (release) [ARM/arm-9-branch revision 277599]
(NOTE gcc version 10 causes Hardfaults at the moment)

To get the tool chain you should do the following on Ubuntu based Linuxes...

    sudo apt-get install gcc-arm-none-eabi

or for Debian Stretch (and Ubuntu) get the tar from here...

   https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads

    Then detar to a directory and do...
        export ARMTOOLS=/downloaddir/gcc-arm-none-eabi-{version}/bin
        (replacing {version} with the version you downloaded)

To build ```cd Firmware; rake -m```

To build unit tests ```cd Firmware; rake testing=1 -m```

To compile only some unit tests in Firmware:

```rake testing=1 test=streams```

```rake testing=1 test=dispatch,streams,planner```

To compile with debug symbols: (may not work as it is very slow)

```rake testing=1 test=streams debug=1```

To compile a unit test that tests a module, and to include that module

```rake testing=1 test=temperatureswitch modules=tools/temperatureswitch```

You need to install ruby (and rake) to build.

```> sudo apt-get install ruby```

## Windows
The rake build system will run on Windows, however some utilities need to be installed first.

1. Install git - https://github.com/git-for-windows/git/releases/download/v2.31.1.windows.1/Git-2.31.1-64-bit.exe (or current version)
2. Install ruby - https://rubyinstaller.org/downloads/
3. (Optionally) Install python3 - https://www.python.org/ftp/python/3.9.5/python-3.9.5-amd64.exe (or current v3 version)
4. Install gcc-arm-none-eabi - https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-win32.zip?revision=ffcaa06c-940d-4319-8d7e-d3070092d392&la=en&hash=130196DDF95B0D9817E1FDE08A725C7EBFBFB5B8 unzip the file and move to ```/usr``` or set the ```ARMTOOLS``` environment variable to where the bin directory is.

make sure they are all added to the PATH, then in powershell navigate to the Firmware folder and type ```rake -m```

