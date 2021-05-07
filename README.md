# smoothie version 2 for STM32H745 (and STM32H743)
Smoothie V2 using STM32 HAL and FreeRTOS.

Currently runs on nucleo 745 board and Devebox 743 board

Firmware/... is for Smoothie firmware code and Test Units

Currently uses the following toolchain..
    gcc version 9.2.1 20191025 (release) [ARM/arm-9-branch revision 277599]

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


