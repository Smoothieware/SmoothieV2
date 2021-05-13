/*
*****************************************************************************
**

**  File        : LinkerScript.ld
**
**  Abstract    : Linker script for STM32H745 Device with
**                2048KByte FLASH, 128KByte RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2017 STMicroelectronics.
** All rights reserved.
**
** This software component is licensed by ST under BSD 3-Clause license,
** the "License"; You may not use this file except in compliance with the
** License. You may obtain a copy of the License at:
**                        opensource.org/licenses/BSD-3-Clause
**
****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the system stack */
_estack = 0x20020000;    /* end of DTCMRAM */
__top_DTCMRAM = _estack - 4096 ; /* guarantee at least 4k of stack */

/* Specify the memory areas */
MEMORY
{
ITCMRAM (xrw)      : ORIGIN = 0x00000000, LENGTH = 64K
FLASH (rx)         : ORIGIN = 0x08000000, LENGTH = 2048K
DTCMRAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K
AXI_SRAM (xrw)     : ORIGIN = 0x24000000, LENGTH = 512K
SRAM_1 (xrw)       : ORIGIN = 0x30000000, LENGTH = 128K
SRAM_2 (xrw)       : ORIGIN = 0x30020000, LENGTH = 128K
SRAM_3 (xrw)       : ORIGIN = 0x30040000, LENGTH = 32K
SRAM_4 (xrw)       : ORIGIN = 0x38000000, LENGTH = 64K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    __vectors_start__ = ABSOLUTE(.) ;
    KEEP(*(.isr_vector)) /* Startup code */
    __vectors_end__ = ABSOLUTE(.) ;
    . = ALIGN(4);
  } >FLASH

  /* modules registration table */
  .modules_table :
  {
    /* *(.after_vectors*) */
    . = ALIGN(4);
    __registered_modules_start = .;
    KEEP(*(.registered_modules*))
    __registered_modules_end = .;
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >AXI_SRAM AT> FLASH


  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >AXI_SRAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    /*. = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);*/
  } >AXI_SRAM

  /* start of heap and end of heap */
  PROVIDE(__HeapBase = _end);
  PROVIDE(__HeapTop = 0x24080000);

  /* copy vector table here */
  .itcm_vectors :
  {
    itcm_vectors_start = .;
    . = . + (__vectors_end__ - __vectors_start__);
    itcm_vectors_end = .;
  } >ITCMRAM

  /* Copy specific fast-executing code to ITCM RAM */
  itcm_data = LOADADDR(.itcm_text);
  .itcm_text :
  {
    . = ALIGN(4);
    itcm_text_start = .;
    *(.ramfunc*)
    . = ALIGN(4);
    itcm_text_end = .;
  } >ITCMRAM AT> FLASH


  /* Copy specific fast data to DTCM RAM */
  dtcm_data = LOADADDR(.dtcm_text);
  .dtcm_text :
  {
    . = ALIGN(4);
    dtcm_text_start = .;
    *(.dtcm_text)
    *(.dtcm_text*)
    . = ALIGN(4);
    dtcm_text_end = .;
  } >DTCMRAM AT> FLASH

  /* force the flashloader bin to the end of the image so we can easily check the magic number */
  .flashload_o :
  {
  	. = ALIGN(4);
  	*(.flashloader_data)
  } >FLASH

 .usb_data (NOLOAD) :
  {
    PROVIDE_HIDDEN (__usb_data_start = .);
    KEEP (*(SORT(.usb_data.*)))
    KEEP (*(.usb_data*))
    PROVIDE_HIDDEN (__usb_data_end = .);
  } >SRAM_1

  .ethernet_data (NOLOAD) :
  {
    PROVIDE_HIDDEN (__ethernet_data_start = .);
    KEEP (*(SORT(.ethernet_data.*)))
    KEEP (*(.ethernet_data*))
    PROVIDE_HIDDEN (__ethernet_data_end = .);
  } >SRAM_2


  /* to take md5sum of the flash .bin */
  _image_start = LOADADDR(.isr_vector);
  _image_end = LOADADDR(.dtcm_text) + SIZEOF(.dtcm_text);

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}


