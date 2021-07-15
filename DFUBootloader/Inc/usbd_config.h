/**
  ******************************************************************************
  * @file    USB_Device/DFU_Standalone/Inc/usbd_conf.h
  * @author  MCD Application Team
  * @brief   General low level driver configuration
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CONF_H
#define __USBD_CONF_H

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Common Config */
#undef USB_OTG_HS

/** @brief Must be set high er than:
 * @arg the length of the entire USB device configuration descriptor
 * @arg the longest USB string (as Unicode string descriptor)
 * @arg the length of the longest class-specific control request */
#define USBD_EP0_BUFFER_SIZE        1024
/** @brief Set to 1 if SET_CONTROL_LINE_STATE request is used by a CDC-ACM interface. */
#define USBD_CDC_CONTROL_LINE_USED  0
/** @brief Set to 1 if notifications are sent by a CDC-ACM interface.
 * In this case notification EP will be allocated and opened if its address is valid. */
#define USBD_CDC_NOTEP_USED         1
/** @brief Must be set according to the highest number of interfaces for a given USB Device. */
#define USBD_MAX_IF_COUNT           8
/** @brief Set to 1 if peripheral support and application demand
 * for High-Speed operation both exist. */
#define USBD_HS_SUPPORT             0

/** @brief When set to 0, no SerialNumber is readable by the host.
 * Otherwise the SerialNumber will be converted from USBD_SERIAL_BCD_SIZE / 2
 * amount of raw bytes to string BCD format and sent to the host. */
#define USBD_SERIAL_BCD_SIZE        0

/** @brief Selects which Microsoft OS descriptor specification should be used (if any).
 * Supported values are: 0, 1, 2
 * @note Microsoft OS 2.0 descriptors are supported by Windows 8.1 and higher.
 * Unless the device is required to operate on earlier Windows OS versions, use version 2. */
#define USBD_MS_OS_DESC_VERSION     0

/** @brief Set to 1 if SET_CONTROL_LINE_STATE request is used by a CDC-ACM interface. */
#define USBD_CDC_CONTROL_LINE_USED  0

/** @brief Set to 1 if SEND_BREAK request is used by a CDC-ACM interface. */
#define USBD_CDC_BREAK_SUPPORT      0



/** @brief Set to 1 if a DFU interface holds more than one applications as alternate settings. */
#define USBD_DFU_ALTSETTINGS        0

/** @brief Set to 1 if DFU STMicroelectronics Extension
 *  protocol (v1.1A) shall be used instead of the standard DFU (v1.1). */
#define USBD_DFU_ST_EXTENSION       0



/** @brief Set to 1 if a HID interface holds more than one applications as alternate settings. */
#define USBD_HID_ALTSETTINGS        0

/** @brief Set to 1 if a HID interface uses an OUT endpoint. */
#define USBD_HID_OUT_SUPPORT        0

/** @brief Set to 1 if a HID interface defines strings in its report descriptor. */
#define USBD_HID_REPORT_STRINGS     0


#endif /* __USBD_CONF_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
