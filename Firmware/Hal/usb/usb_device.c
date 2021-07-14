/**
  ******************************************************************************
  * @file    usb_device.c
  * @author  Benedek Kupper
  * @version 0.1
  * @date    2018-11-03
  * @brief   USB device definition and initialization
  *
  * Copyright (c) 2018 Benedek Kupper
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */
#include "usb_device.h"
#include "usbd_cdc.h"
#include "stm32_rom_dfu.h"

int config_dfu_required = 1;

/** @brief USB device configuration */
const USBD_DescriptionType hdev_cfg = {
    .Vendor = {
        .Name           = "SmoothieWare",
        .ID             = 0x1d50,
    },
    .Product = {
        .Name           = "Smoothie CDC/DFU",
        .ID             = 0x6015,
        .Version.bcd    = 0x0100,
    },
    .Config = {
        .Name           = "Smoothie config",
        .MaxCurrent_mA  = 100,
        .RemoteWakeup   = 0,
        .SelfPowered    = 1,
    },
}, *const dev_cfg = &hdev_cfg;

USBD_HandleType hUsbDevice, *const UsbDevice = &hUsbDevice;

extern USBD_CDC_IfHandleType *const vcom_if;

void UsbDevice_Init(void)
{
    if(config_dfu_required) {
        if(USBD_DFU_MountRebootOnly(stm32_rom_dfu_if, UsbDevice) != USBD_E_OK) {
            printf("ERROR: USBD_DFU_MountRebootOnly failed\n");
        }
    }

    /* All fields of Config have to be properly set up */
    vcom_if->Config.InEpNum  = 0x81;
    vcom_if->Config.OutEpNum = 0x01;
    vcom_if->Config.NotEpNum = 0x82;

    /* Mount the interfaces to the device */
    if(USBD_CDC_MountInterface(vcom_if, UsbDevice) != USBD_E_OK) {
        printf("ERROR: USBD_CDC_MountInterface failed\n");
    }

    /* Initialize the device */
    USBD_Init(UsbDevice, dev_cfg);

    /* The device connection can be made */
    USBD_Connect(UsbDevice);
}

void UsbDevice_DeInit(void)
{
    USBD_Deinit(UsbDevice);
}
