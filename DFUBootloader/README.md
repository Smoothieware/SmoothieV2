This is an optional DFU bootloader for SmoothieV2.
This is mainly for developers.

If can be built to load into QSPI or at the end of normal flash memory.

To build: 
    
    cd DFUBootloader
    rake target=Prime -m

When flashed and dfu_enable is selected in config.ini you can run dfu-util to flash new firmware.

When built for a board with QSPI (Smoothieboard Prime V2, Devebox) then the dfuloader.bin should be flashed to address 0x90000000, which can be done within smoothie by...

1. copy file dfuloader.bin to the sdcard
2. from the console type ```qspi flash dfuloader.bin```

Once this is done it can be run using the dfu command or the qspi run command of just run dfu-util

When built for a board without QSPI (Nucleo) then the dfuloader.bin should be flashed at address 0x081E0000. This can be done with a Jlink and the FlashLite program, or using another device capable of flashing a binary to an arbitrary address.

Running smoothie for the Nucleo the dfu command will run the dfu flasher as will running dfu-util.
