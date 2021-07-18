/**
  ******************************************************************************
  * @file    stm32h743i_eval_sd.c
  * @author  MCD Application Team
  * @brief   This file includes the uSD card driver mounted on STM32H743I-EVAL
  *          evaluation boards.
  @verbatim
  How To use this driver:
  -----------------------
  - This driver is used to drive the micro SD external cards mounted on STM32H743I-EVAL
  evaluation board.
  - This driver does not need a specific component driver for the micro SD device
  to be included with.

  Driver description:
  ------------------
  + Initialization steps:
  o Initialize the micro SD card using the BSP_SD_Init() function. This
  function includes the MSP layer hardware resources initialization and the
  SDIO interface configuration to interface with the external micro SD. It
  also includes the micro SD initialization sequence for SDCard1.
  o To check the SD card presence you can use the function BSP_SD_IsDetected() which
  returns the detection status for SDCard1. The function BSP_SD_IsDetected() returns
  the detection status for SDCard1.
  o If SD presence detection interrupt mode is desired, you must configure the
  SD detection interrupt mode by calling the functions BSP_SD_ITConfig() for
  SDCard1. The interrupt is generated as an external interrupt whenever the
  micro SD card is plugged/unplugged in/from the evaluation board. The SD detection
  is managed by MFX, so the SD detection interrupt has to be treated by MFX_IRQOUT
  gpio pin IRQ handler.
  o The function BSP_SD_GetCardInfo() are used to get the micro SD card information
  which is stored in the structure "HAL_SD_CardInfoTypedef".

  + Micro SD card operations
  o The micro SD card can be accessed with read/write block(s) operations once
  it is ready for access. The access, by default to SDCard1, can be performed whether
  using the polling mode by calling the functions BSP_SD_ReadBlocks()/BSP_SD_WriteBlocks(),
  or by DMA transfer using the functions BSP_SD_ReadBlocks_DMA()/BSP_SD_WriteBlocks_DMA().
  o The DMA transfer complete is used with interrupt mode. Once the SD transfer
  is complete, the SD interrupt is handled using the function BSP_SDMMC1_IRQHandler()
  when SDCard1 is used.
  o The SD erase block(s) is performed using the functions BSP_SD_Erase() with specifying
  the number of blocks to erase.
  o The SD runtime status is returned when calling the function BSP_SD_GetStatus().

  @endverbatim
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_sd.h"
#include "Hal_pin.h"

#include "FreeRTOS.h"

extern void BSP_SD_DetectCallback();

#define BSP_SD_IT_PRIORITY  configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY

SD_HandleTypeDef hsd_sdmmc[SD_INSTANCES_NBR];
EXTI_HandleTypeDef hsd_exti[SD_INSTANCES_NBR];

static void SD_MspInit(SD_HandleTypeDef *hsd);
static void SD_MspDeInit(SD_HandleTypeDef *hsd);
#if (USE_HAL_SD_REGISTER_CALLBACKS == 1)
static void SD_AbortCallback(SD_HandleTypeDef *hsd);
static void SD_TxCpltCallback(SD_HandleTypeDef *hsd);
static void SD_RxCpltCallback(SD_HandleTypeDef *hsd);
#if (USE_SD_TRANSCEIVER != 0U)
static void SD_DriveTransceiver_1_8V_Callback(FlagStatus status);
#endif
#endif /* (USE_HAL_SD_REGISTER_CALLBACKS == 1)   */

int32_t BSP_SD_Init(uint32_t Instance)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else {
#ifndef BOARD_DEVEBOX
        // setup sd detect pin
        SD_DETECT_GPIO_CLK_ENABLE();
        GPIO_InitTypeDef gpio_init_structure;
        gpio_init_structure.Pin = SD_DETECT_PIN;
        gpio_init_structure.Pull = GPIO_PULLUP;
        gpio_init_structure.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_structure.Mode = GPIO_MODE_IT_RISING_FALLING;
        gpio_init_structure.Alternate = 0;
        HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &gpio_init_structure);
        allocate_hal_pin(SD_DETECT_GPIO_PORT, SD_DETECT_PIN);
        allocate_hal_interrupt_pin(SD_DETECT_PIN, BSP_SD_DetectCallback);
#endif
        /* Check if SD card is present   */

        if((uint32_t)BSP_SD_IsDetected(Instance) != SD_PRESENT) {
            ret = BSP_ERROR_UNKNOWN_COMPONENT;
        } else {

#if (USE_HAL_SD_REGISTER_CALLBACKS == 1)
            /* Register the SD MSP Callbacks   */
            if(IsMspCallbacksValid[Instance] == 0UL) {
                if(BSP_SD_RegisterDefaultMspCallbacks(Instance) != BSP_ERROR_NONE) {
                    ret = BSP_ERROR_PERIPH_FAILURE;
                }
            }
#else
            /* Msp SD initialization   */
            SD_MspInit(&hsd_sdmmc[Instance]);
#endif /* USE_HAL_SD_REGISTER_CALLBACKS   */

            if(ret == BSP_ERROR_NONE) {
                /* HAL SD initialization and Enable wide operation   */
                if(MX_SDMMC1_SD_Init(&hsd_sdmmc[Instance]) != HAL_OK) {
                    ret = BSP_ERROR_PERIPH_FAILURE;
                } else if(HAL_SD_ConfigWideBusOperation(&hsd_sdmmc[Instance], SDMMC_BUS_WIDE_4B) != HAL_OK) {
                    ret = BSP_ERROR_PERIPH_FAILURE;
                } else {
                    /* Switch to High Speed mode if the card support this mode */
                    (void)HAL_SD_ConfigSpeedBusOperation(&hsd_sdmmc[Instance], SDMMC_SPEED_MODE_HIGH);

#if (USE_HAL_SD_REGISTER_CALLBACKS == 1)
                    /* Register SD TC, HT and Abort callbacks */
                    if(HAL_SD_RegisterCallback(&hsd_sdmmc[Instance], HAL_SD_TX_CPLT_CB_ID, SD_TxCpltCallback) != HAL_OK) {
                        ret = BSP_ERROR_PERIPH_FAILURE;
                    } else if(HAL_SD_RegisterCallback(&hsd_sdmmc[Instance], HAL_SD_RX_CPLT_CB_ID, SD_RxCpltCallback) != HAL_OK) {
                        ret = BSP_ERROR_PERIPH_FAILURE;
                    } else if(HAL_SD_RegisterCallback(&hsd_sdmmc[Instance], HAL_SD_ABORT_CB_ID, SD_AbortCallback) != HAL_OK) {
                        ret = BSP_ERROR_PERIPH_FAILURE;
                    } else {
#if (USE_SD_TRANSCEIVER != 0U)
                        if(HAL_SD_RegisterTransceiverCallback(&hsd_sdmmc[Instance], SD_DriveTransceiver_1_8V_Callback) != HAL_OK) {
                            ret = BSP_ERROR_PERIPH_FAILURE;
                        }
#endif
                    }
#endif /* USE_HAL_SD_REGISTER_CALLBACKS   */
                }
            }
        }
    }


    return ret;
}

/**
  * @brief  DeInitializes the SD card device.
  * @param Instance      SD Instance
  * @retval SD status
  */
int32_t BSP_SD_DeInit(uint32_t Instance)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else if(HAL_SD_DeInit(&hsd_sdmmc[Instance]) != HAL_OK) { /* HAL SD de-initialization   */
        ret = BSP_ERROR_PERIPH_FAILURE;

    } else {
        /* Msp SD de-initialization   */
#if (USE_HAL_SD_REGISTER_CALLBACKS == 0)
        SD_MspDeInit(&hsd_sdmmc[Instance]);
#endif /* (USE_HAL_SD_REGISTER_CALLBACKS == 0)   */
    }

    return ret;
}

/**
  * @brief  Initializes the SDMMC1 peripheral.
  * @param  hsd SD handle
  * @retval HAL status
  */
__weak HAL_StatusTypeDef MX_SDMMC1_SD_Init(SD_HandleTypeDef *hsd)
{
    HAL_StatusTypeDef ret = HAL_OK;
    /* uSD device interface configuration */
    hsd->Instance                 = SDMMC1;
    hsd->Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd->Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd->Init.BusWide             = SDMMC_BUS_WIDE_4B;
    hsd->Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    // hsd->Init.TranceiverPresent   = SDMMC_TRANSCEIVER_PRESENT;
#if ( USE_SD_HIGH_PERFORMANCE > 0 )
    hsd->Init.ClockDiv            = SDMMC_HSpeed_CLK_DIV;
#else
    hsd->Init.ClockDiv            = SDMMC_NSpeed_CLK_DIV;
#endif

    /* HAL SD initialization   */
    if(HAL_SD_Init(hsd) != HAL_OK) {
        ret = HAL_ERROR;
    }

    return ret;
}

#if (USE_HAL_SD_REGISTER_CALLBACKS == 1)
/**
  * @brief Default BSP SD Msp Callbacks
  * @param Instance      SD Instance
  * @retval BSP status
  */
int32_t BSP_SD_RegisterDefaultMspCallbacks(uint32_t Instance)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else {
        /* Register MspInit/MspDeInit Callbacks */
        if(HAL_SD_RegisterCallback(&hsd_sdmmc[Instance], HAL_SD_MSP_INIT_CB_ID, SD_MspInit) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        } else if(HAL_SD_RegisterCallback(&hsd_sdmmc[Instance], HAL_SD_MSP_DEINIT_CB_ID, SD_MspDeInit) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        } else {
            IsMspCallbacksValid[Instance] = 1U;
        }
    }
    /* Return BSP status */
    return ret;
}

/**
  * @brief BSP SD Msp Callback registering
  * @param Instance     SD Instance
  * @param CallBacks    pointer to MspInit/MspDeInit callbacks functions
  * @retval BSP status
  */
int32_t BSP_SD_RegisterMspCallbacks(uint32_t Instance, BSP_SD_Cb_t *CallBacks)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else {
        /* Register MspInit/MspDeInit Callbacks */
        if(HAL_SD_RegisterCallback(&hsd_sdmmc[Instance], HAL_SD_MSP_INIT_CB_ID, CallBacks->pMspInitCb) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        } else if(HAL_SD_RegisterCallback(&hsd_sdmmc[Instance], HAL_SD_MSP_DEINIT_CB_ID, CallBacks->pMspDeInitCb) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        } else {
            IsMspCallbacksValid[Instance] = 1U;
        }
    }

    /* Return BSP status */
    return ret;
}
#endif /* (USE_HAL_SD_REGISTER_CALLBACKS == 1) */


/**
  * @brief  Detects if SD card is correctly plugged in the memory slot or not.
  * @param Instance  SD Instance
  * @retval Returns if SD is detected or not
  */
int32_t BSP_SD_IsDetected(uint32_t Instance)
{
#ifndef BOARD_DEVEBOX
    int32_t ret = BSP_ERROR_UNKNOWN_FAILURE;

    if(Instance >= SD_INSTANCES_NBR) {
        return BSP_ERROR_WRONG_PARAM;
    } else {
        /* Check SD card detect pin */
        if(HAL_GPIO_ReadPin(SD_DETECT_GPIO_PORT, SD_DETECT_PIN) == GPIO_PIN_RESET) {
            ret = (int32_t)SD_NOT_PRESENT;
        } else {
            ret = (int32_t)SD_PRESENT;
        }
    }

    return ret;
#else
    return (int32_t)SD_PRESENT;
#endif
}

/**
  * @brief  Reads block(s) from a specified address in an SD card, in DMA mode.
  * @param  Instance   SD Instance
  * @param  pData      Pointer to the buffer that will contain the data to transmit
  * @param  BlockIdx   Block index from where data is to be read
  * @param  BlocksNbr  Number of SD blocks to read
  * @retval BSP status
  */
int32_t BSP_SD_ReadBlocks_DMA(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t BlocksNbr)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else {
        if(HAL_SD_ReadBlocks_DMA(&hsd_sdmmc[Instance], (uint8_t *)pData, BlockIdx, BlocksNbr) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        }
    }

    /* Return BSP status   */
    return ret;
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in DMA mode.
  * @param  Instance   SD Instance
  * @param  pData      Pointer to the buffer that will contain the data to transmit
  * @param  BlockIdx   Block index from where data is to be written
  * @param  BlocksNbr  Number of SD blocks to write
  * @retval BSP status
  */
int32_t BSP_SD_WriteBlocks_DMA(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t BlocksNbr)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else {
        if(HAL_SD_WriteBlocks_DMA(&hsd_sdmmc[Instance], (uint8_t *)pData, BlockIdx, BlocksNbr) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        }
    }

    /* Return BSP status   */
    return ret;
}

/**
  * @brief  Erases the specified memory area of the given SD card.
  * @param  Instance   SD Instance
  * @param  BlockIdx   Block index from where data is to be
  * @param  BlocksNbr  Number of SD blocks to erase
  * @retval SD status
  */
int32_t BSP_SD_Erase(uint32_t Instance, uint32_t BlockIdx, uint32_t BlocksNbr)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else {
        if(HAL_SD_Erase(&hsd_sdmmc[Instance], BlockIdx, BlockIdx + BlocksNbr) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        }
    }

    /* Return BSP status   */
    return ret;
}

/**
  * @brief  Gets the current SD card data status.
  * @param  Instance  SD Instance
  * @retval Data transfer state.
  *          This value can be one of the following values:
  *            @arg  SD_TRANSFER_OK: No data transfer is acting
  *            @arg  SD_TRANSFER_BUSY: Data transfer is acting
  */
int32_t BSP_SD_GetCardState(uint32_t Instance)
{
    return (int32_t)((HAL_SD_GetCardState(&hsd_sdmmc[Instance]) == HAL_SD_CARD_TRANSFER ) ? SD_TRANSFER_OK : SD_TRANSFER_BUSY);
}

/**
  * @brief  Get SD information about specific SD card.
  * @param  Instance  SD Instance
  * @param  CardInfo  Pointer to HAL_SD_CardInfoTypedef structure
  * @retval BSP status
  */
int32_t BSP_SD_GetCardInfo(uint32_t Instance, BSP_SD_CardInfo *CardInfo)
{
    int32_t ret = BSP_ERROR_NONE;

    if(Instance >= SD_INSTANCES_NBR) {
        ret = BSP_ERROR_WRONG_PARAM;
    } else {
        if(HAL_SD_GetCardInfo(&hsd_sdmmc[Instance], CardInfo) != HAL_OK) {
            ret = BSP_ERROR_PERIPH_FAILURE;
        }
    }
    /* Return BSP status */
    return ret;
}

#if !defined (USE_HAL_SD_REGISTER_CALLBACKS) || (USE_HAL_SD_REGISTER_CALLBACKS == 0)
/**
  * @brief SD Abort callbacks
  * @param hsd  SD handle
  * @retval None
  */
void HAL_SD_AbortCallback(SD_HandleTypeDef *hsd)
{
    BSP_SD_AbortCallback((hsd == &hsd_sdmmc[0]) ? 0UL : 1UL);
}

/**
  * @brief Tx Transfer completed callbacks
  * @param hsd  SD handle
  * @retval None
  */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    BSP_SD_WriteCpltCallback((hsd == &hsd_sdmmc[0]) ? 0UL : 1UL);
}

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd  SD handle
  * @retval None
  */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    BSP_SD_ReadCpltCallback((hsd == &hsd_sdmmc[0]) ? 0UL : 1UL);
}

/**
  * @brief SD error callbacks
  * @param hsd: Pointer SD handle
  * @retval None
  */
#include <stdio.h>
void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
	printf("BSP_SD_ErrorCallback: %04lX\n", hsd->ErrorCode);
}

#if (USE_SD_TRANSCEIVER != 0U)
/**
  * @brief  Enable the SD Transceiver 1.8V Mode Callback.
  */
void HAL_SD_DriveTransciver_1_8V_Callback(FlagStatus status)
{
#if (USE_BSP_IO_CLASS > 0U)
    if(status == SET) {
        BSP_IO_WritePin(0, SD_LDO_SEL_PIN, IO_PIN_SET);
    } else {
        BSP_IO_WritePin(0, SD_LDO_SEL_PIN, IO_PIN_RESET);
    }
#endif
}
#endif
#endif /* !defined (USE_HAL_SD_REGISTER_CALLBACKS) || (USE_HAL_SD_REGISTER_CALLBACKS == 0)   */

/**
  * @brief  This function handles SDMMC interrupt requests.
  * @param  Instance  SD Instance
  * @retval None
  */
void BSP_SD_IRQHandler(uint32_t Instance)
{
    HAL_SD_IRQHandler(&hsd_sdmmc[Instance]);
}

/**
  * @brief BSP SD Abort callbacks
  * @param  Instance     SD Instance
  * @retval None
  */
__weak void BSP_SD_AbortCallback(uint32_t Instance)
{
    /* Prevent unused argument(s) compilation warning   */
    UNUSED(Instance);
}

/**
  * @brief BSP Tx Transfer completed callbacks
  * @param  Instance     SD Instance
  * @retval None
  */
__weak void BSP_SD_WriteCpltCallback(uint32_t Instance)
{
    /* Prevent unused argument(s) compilation warning   */
    UNUSED(Instance);
}

/**
  * @brief BSP Rx Transfer completed callbacks
  * @param  Instance     SD Instance
  * @retval None
  */
__weak void BSP_SD_ReadCpltCallback(uint32_t Instance)
{
    /* Prevent unused argument(s) compilation warning   */
    UNUSED(Instance);
}

/**
  * @}
  */

/** @defgroup STM32H747I_EVAL_SD_Private_Functions Private Functions
  * @{
  */
#if (USE_HAL_SD_REGISTER_CALLBACKS == 1)
/**
  * @brief SD Abort callbacks
  * @param hsd  SD handle
  * @retval None
  */
static void SD_AbortCallback(SD_HandleTypeDef *hsd)
{
    BSP_SD_AbortCallback((hsd == &hsd_sdmmc[0]) ? 0UL : 1UL);
}

/**
  * @brief Tx Transfer completed callbacks
  * @param hsd  SD handle
  * @retval None
  */
static void SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    BSP_SD_WriteCpltCallback((hsd == &hsd_sdmmc[0]) ? 0UL : 1UL);
}

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd  SD handle
  * @retval None
  */
static void SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    BSP_SD_ReadCpltCallback((hsd == &hsd_sdmmc[0]) ? 0UL : 1UL);
}

#if (USE_SD_TRANSCEIVER != 0U)
/**
  * @brief  Enable the SD Transciver 1.8V Mode Callback.
  * @param  status Transceiver 1.8V status
  * @retval None
  */
static void SD_DriveTransceiver_1_8V_Callback(FlagStatus status)
{
#if (USE_BSP_IO_CLASS > 0U)
    if(status == SET) {
        BSP_IO_WritePin(0, SD_LDO_SEL_PIN, IO_PIN_SET);
    } else {
        BSP_IO_WritePin(0, SD_LDO_SEL_PIN, IO_PIN_RESET);
    }
#endif
}
#endif
#endif/* (USE_HAL_SD_REGISTER_CALLBACKS == 1) */

/**
  * @brief  Initializes the SD MSP.
  * @param  hsd  SD handle
  * @retval None
  */
static void SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef gpio_init_structure;

    if(hsd == &hsd_sdmmc[0]) {
        /* Enable SDIO clock */
        __HAL_RCC_SDMMC1_CLK_ENABLE();

        /* Enable GPIOs clock */
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /* Common GPIO configuration */
        gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
        gpio_init_structure.Pull      = GPIO_NOPULL;
        gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH; // GPIO_SPEED_FREQ_VERY_HIGH;
        /* D0(PC8), D1(PC9), D2(PC10), D3(PC11), CK(PC12), CMD(PD2) */
        /* Common GPIO configuration */
        gpio_init_structure.Alternate = GPIO_AF12_SDIO1;
        /* GPIOC configuration */
        gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        HAL_GPIO_Init(GPIOC, &gpio_init_structure);
        /* GPIOD configuration */
        gpio_init_structure.Pin = GPIO_PIN_2;
        HAL_GPIO_Init(GPIOD, &gpio_init_structure);

        allocate_hal_pin(GPIOC, GPIO_PIN_8);
        allocate_hal_pin(GPIOC, GPIO_PIN_9);
        allocate_hal_pin(GPIOC, GPIO_PIN_10);
        allocate_hal_pin(GPIOC, GPIO_PIN_11);
        allocate_hal_pin(GPIOC, GPIO_PIN_12);
        allocate_hal_pin(GPIOD, GPIO_PIN_2);

#if 0
        /* D0DIR(PC6), D123DIR(PC7) */
        gpio_init_structure.Alternate = GPIO_AF8_SDIO1;
        /* GPIOC configuration */
        gpio_init_structure.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOC, &gpio_init_structure);
        /* CKIN(PB8), CDIR(PB9) */
        gpio_init_structure.Alternate = GPIO_AF7_SDIO1;
        /* GPIOB configuration */
        gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9;
        HAL_GPIO_Init(GPIOB, &gpio_init_structure);
#endif
        __HAL_RCC_SDMMC1_FORCE_RESET();
        __HAL_RCC_SDMMC1_RELEASE_RESET();

        /* NVIC configuration for SDIO interrupts   */
        NVIC_SetPriority(SDMMC1_IRQn, BSP_SD_IT_PRIORITY);
        NVIC_EnableIRQ(SDMMC1_IRQn);
    }
}

/**
  * @brief  DeInitializes the SD MSP.
  * @param  hsd  SD handle
  * @retval None
  */
static void SD_MspDeInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef gpio_init_structure;

    if(hsd == &hsd_sdmmc[0]) {
        /* Disable NVIC for SDIO interrupts   */
        HAL_NVIC_DisableIRQ(SDMMC1_IRQn);

        /* GPIOB configuration   */
        gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9;
        HAL_GPIO_DeInit(GPIOB, gpio_init_structure.Pin);

        /* GPIOC configuration   */
        gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        HAL_GPIO_DeInit(GPIOC, gpio_init_structure.Pin);

        /* GPIOC configuration   */
        gpio_init_structure.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_DeInit(GPIOC, gpio_init_structure.Pin);

        /* GPIOD configuration   */
        gpio_init_structure.Pin = GPIO_PIN_2;
        HAL_GPIO_DeInit(GPIOD, gpio_init_structure.Pin);

        /* Disable SDMMC1 clock   */
        __HAL_RCC_SDMMC1_CLK_DISABLE();
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE***  */
