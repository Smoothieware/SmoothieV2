/**
  ******************************************************************************
  * @file    stm32h743i_eval_config.h
  * @author  MCD Application Team
  * @brief   STM32h743i_EVAL board configuration file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2018 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32h743i_EVAL_CONFIG_H
#define STM32h743i_EVAL_CONFIG_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

#define BSP_SD_IT_PRIORITY                  14U
#define BSP_SD_RX_IT_PRIORITY               14U
#define BSP_SD_TX_IT_PRIORITY               15U

#ifdef __cplusplus
}
#endif

#endif /* STM32h743i_EVAL_CONFIG_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
