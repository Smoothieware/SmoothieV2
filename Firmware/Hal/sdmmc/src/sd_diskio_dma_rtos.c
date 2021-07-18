/**
  ******************************************************************************
  * @file    sd_diskio_dma_rtos_template.c
  * @author  MCD Application Team
  * @brief   SD Disk I/O DMA with RTOS driver.
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

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "stm32h7xx_sd.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>

extern const Diskio_drvTypeDef  SD_Driver;


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define QUEUE_SIZE         (uint32_t) 10
#define READ_CPLT_MSG      (uint32_t) 1
#define WRITE_CPLT_MSG     (uint32_t) 2

/*
 * the following Timeout is useful to give the control back to the applications
 * in case of errors in either BSP_SD_ReadCpltCallback() or BSP_SD_WriteCpltCallback()
 * the value by default is as defined in the BSP platform driver otherwise 30 secs
 *
 */

#define SD_TIMEOUT pdMS_TO_TICKS(30 * 1000)

#define SD_DEFAULT_BLOCK_SIZE 512

/*
 * Depending on the usecase, the SD card initialization could be done at the
 * application level, if it is the case define the flag below to disable
 * the BSP_SD_Init() call in the SD_Initialize().
 */

/* #define DISABLE_SD_INIT */


/*
 * when using cacheable memory region, it may be needed to maintain the cache
 * validity. Enable the define below to activate a cache maintenance at each
 * read and write operation.
 * Notice: This is applicable only for cortex M7 based platform.
 -- Need when MMU sets memory to writethrough but cacheable
 */

#define ENABLE_SD_DMA_CACHE_MAINTENANCE 1

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
static QueueHandle_t SDQueueID;

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
DRESULT SD_ioctl (BYTE, BYTE, void*);
#endif  /* _USE_IOCTL == 1 */

const Diskio_drvTypeDef  SD_Driver = {
    SD_initialize,
    SD_status,
    SD_read,
#if  _USE_WRITE == 1
    SD_write,
#endif /* _USE_WRITE == 1 */

#if  _USE_IOCTL == 1
    SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun)
{
    Stat = STA_NOINIT;

    if(BSP_SD_GetCardState(0) == BSP_ERROR_NONE) {
        Stat &= ~STA_NOINIT;
    }

    return Stat;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun)
{
    Stat = STA_NOINIT;

    if(BSP_SD_Init(0) == BSP_ERROR_NONE) {
        Stat = SD_CheckStatus(lun);
    }

    /*
     * if the SD is correctly initialized, create the operation queue
     */

    if (Stat != STA_NOINIT) {
        SDQueueID = xQueueCreate(QUEUE_SIZE, sizeof(uint16_t));
    }

    return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
    return SD_CheckStatus(lun);
}

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res = RES_ERROR;
    uint16_t event;
    uint32_t timer;
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
    uint32_t alignedAddr;
#endif
    uint32_t *aligned_buffer = (uint32_t*)buff;
    int32_t cbRead = 0;

    // DMA needs to be on aligned 32bit boundaries
    if(((uint32_t)buff & 0x03) != 0) {
        // unaligned buffer is not ok
        cbRead = count * FF_MAX_SS;
        aligned_buffer = (uint32_t *)malloc(cbRead);
        if (aligned_buffer == NULL) {
            return RES_ERROR;
        }
    }

#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
    // make sure cache is flushed and invalidated so when DMA updates the RAM
    // from reading the peripheral the CPU then reads the new data
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)((uint32_t)aligned_buffer & ~0x1f),
                                      ((uint32_t)((uint8_t *)aligned_buffer + (count * FF_MAX_SS) + 0x1f) & ~0x1f) -
                                      ((uint32_t)aligned_buffer & ~0x1f));
#endif

    if(BSP_SD_ReadBlocks_DMA(0, aligned_buffer,
                             (uint32_t) (sector),
                             count) == BSP_ERROR_NONE) {
        /* wait for a message from the queue or a timeout */
        if (xQueueReceive(SDQueueID, &event, SD_TIMEOUT)) {
            if(event == READ_CPLT_MSG) {
                timer = xTaskGetTickCount() + SD_TIMEOUT;
                /* block until SDIO IP is ready or a timeout occur */
                while(timer > xTaskGetTickCount()) {
                    if (BSP_SD_GetCardState(0) == SD_TRANSFER_OK) {
                        res = RES_OK;
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
                        // invalidate again just in case some other thread touched the cache
                        alignedAddr = (uint32_t)aligned_buffer & ~0x1F;
                        SCB_InvalidateDCache_by_Addr((uint32_t*)alignedAddr, count * BLOCKSIZE + ((uint32_t)aligned_buffer - alignedAddr));
#endif
                        break;
                    }
                }
            }
        }
    }

    // we need to copy the aligned buffer to the requested buffer
    if(cbRead != 0) {
        memcpy(buff, aligned_buffer, cbRead);
        free(aligned_buffer);
    }

    return res;
}

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    uint16_t event;
    DRESULT res = RES_ERROR;
    uint32_t timer;
    uint32_t *aligned_buffer = (uint32_t*)buff;
    int32_t cbWrite = 0;

    // DMA needs to be on aligned 32bit boundaries
    if(((uint32_t)buff & 0x03) != 0) {
        // unaligned buffer is not ok
        cbWrite = count * FF_MAX_SS;
        aligned_buffer = (uint32_t *)malloc(cbWrite);
        if (aligned_buffer == NULL) {
            return RES_ERROR;
        }
        memcpy(aligned_buffer, buff, cbWrite);
    }

#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
    // make sure cache is flushed to RAM so the DMA can read the correct data
    SCB_CleanDCache_by_Addr((uint32_t *)((uint32_t)aligned_buffer & ~0x1f),
                            ((uint32_t)((uint8_t *)aligned_buffer + (count * FF_MAX_SS) + 0x1f) & ~0x1f) -
                            ((uint32_t)aligned_buffer & ~0x1f));
#endif


    if(BSP_SD_WriteBlocks_DMA(0, aligned_buffer,
                              (uint32_t) (sector),
                              count) == BSP_ERROR_NONE) {

        /* Get the message from the queue */
        if (xQueueReceive(SDQueueID, &event, SD_TIMEOUT)) {
            if (event == WRITE_CPLT_MSG) {
                timer = xTaskGetTickCount() + SD_TIMEOUT;
                /* block until SDIO IP is ready or a timeout occur */
                while(timer > xTaskGetTickCount()) {
                    if (BSP_SD_GetCardState(0) == SD_TRANSFER_OK) {
                        res = RES_OK;
                        break;
                    }
                }
            }
        }
    }

    if(cbWrite != 0) {
        free(aligned_buffer);
    }

    return res;
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;
    BSP_SD_CardInfo CardInfo;

    if (Stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd) {
        /* Make sure that no pending write process */
        case CTRL_SYNC :
            res = RES_OK;
            break;

        /* Get number of sectors on the disk (DWORD) */
        case GET_SECTOR_COUNT :
            BSP_SD_GetCardInfo(0, &CardInfo);
            *(DWORD*)buff = CardInfo.LogBlockNbr;
            res = RES_OK;
            break;

        /* Get R/W sector size (WORD) */
        case GET_SECTOR_SIZE :
            BSP_SD_GetCardInfo(0, &CardInfo);
            *(WORD*)buff = CardInfo.LogBlockSize;
            res = RES_OK;
            break;

        /* Get erase block size in unit of sector (DWORD) */
        case GET_BLOCK_SIZE :
            BSP_SD_GetCardInfo(0, &CardInfo);
            *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
            res = RES_OK;
            break;

        default:
            res = RES_PARERR;
    }

    return res;
}
#endif /* _USE_IOCTL == 1 */



/**
  * @brief Tx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
int sd_no_rtos= 0;
volatile int sd_write_ready;
void BSP_SD_WriteCpltCallback(uint32_t Instance)
{
	if(sd_no_rtos > 0) {
		sd_write_ready= 1;
		return;
	}
    // We have not woken a task at the start of the ISR.
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint16_t v = WRITE_CPLT_MSG;
    xQueueSendFromISR(SDQueueID, &v, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

volatile int sd_read_ready;
void BSP_SD_ReadCpltCallback(uint32_t Instance)
{
	if(sd_no_rtos > 0) {
		sd_read_ready= 1;
		return;
	}
    // We have not woken a task at the start of the ISR.
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint16_t v = READ_CPLT_MSG;
    xQueueSendFromISR(SDQueueID, &v, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR (xHigherPriorityTaskWoken);
}

/**
  * @brief  This function handles SDIO interrupt request.
  * @param  None
  * @retval None
  */
void SDMMC1_IRQHandler(void)
{
    BSP_SD_IRQHandler(0);
}
