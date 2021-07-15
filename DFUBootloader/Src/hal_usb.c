/**
  ******************************************************************************
  * @file    hal_usb.c
  * @author  Benedek Kupper
  * @version 0.1
  * @date    2018-11-03
  * @brief   HAL adapter for USBDevice
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
#include "stm32h7xx_hal.h"

#include <xpd_usb.h>
#include <usb_device.h>

void HAL_USB_OTG_FS_MspInit(void* handle)
{

	GPIO_InitTypeDef GPIO_InitStruct;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Configure DM DP Pins */
    GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure VBUS Pin */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure ID pin */
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Enable USB FS Clocks */
    __HAL_RCC_USB2_OTG_FS_CLK_ENABLE();

	__HAL_RCC_USB2_OTG_FS_CLK_SLEEP_ENABLE();
    /* Disable USB clock during CSleep mode */
    __HAL_RCC_USB2_OTG_FS_ULPI_CLK_SLEEP_DISABLE();

    /* Set USBFS Interrupt to the lowest priority */
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);

    /* Enable USBFS Interrupt */
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

}

void HAL_USB_OTG_FS_MspDeInit(void* handle)
{
    /* Disable USB FS Clocks */
    __HAL_RCC_USB2_OTG_FS_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
}

void HAL_USBD_Setup(void)
{
    USB_INST2HANDLE(UsbDevice, USB_OTG_FS);
    UsbDevice->Callbacks.DepInit = HAL_USB_OTG_FS_MspInit;
    UsbDevice->Callbacks.DepDeinit = HAL_USB_OTG_FS_MspDeInit;
}

void OTG_FS_IRQHandler(void)
{
    USB_vIRQHandler(UsbDevice);
}
