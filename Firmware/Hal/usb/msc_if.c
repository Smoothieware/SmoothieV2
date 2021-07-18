#include "msc_if.h"
#include "stm32h7xx_sd.h"

#define FLASH_BLK_NBR 100
#define FLASH_BLK_SIZ 512

USBD_ReturnType Read_dev (uint8_t lun, uint8_t *dest, uint32_t blockAddr, uint16_t blockLen);
USBD_ReturnType Write_dev (uint8_t lun, uint8_t *src, uint32_t blockAddr, uint16_t blockLen);
void Init_dev(uint8_t lun);
void DeInit_dev(uint8_t lun);

USBD_MSC_LUStatusType Status_dev =  {
    .BlockCount = FLASH_BLK_NBR,
    .BlockSize = FLASH_BLK_SIZ,
    .Ready = 0,
    .Writable = 0,
};

const USBD_SCSI_StdInquiryType Inquiry_dev = {
    .PeriphType = SCSI_PERIPH_SBC_2,
    .RMB = 1,
    .Version = 2,
    .RespDataFormat = 2,
    .AddLength = sizeof(USBD_SCSI_StdInquiryType) - 4,
    .VendorId = "Smoothie",
    .ProductId = "Mass Storage",
    .VersionId = "1.00"
};

USBD_MSC_LUType msc_lu[] = {
    {
        .Read = Read_dev,
        .Write = Write_dev,
        .Status = &Status_dev,
        .Inquiry = &Inquiry_dev,
        .Init = Init_dev,
        .Deinit = DeInit_dev,
    },
};

USBD_MSC_IfHandleType hmsc_if = {
    .Base.AltCount = 1,
    .LUs = msc_lu,
}, *const msc_if = &hmsc_if;

static uint16_t blocksize;

extern int sd_no_rtos;
/**
  * @brief  Initializes the storage unit (medium)
  * @param  lun: Logical unit number
  * @retval Status (0 : OK / -1 : Error)
  */
int8_t STORAGE_Init(uint8_t lun)
{
    sd_no_rtos = 1; // tells BSP to not use queue on cmplt interrupts
    return 0;
}

/**
  * @brief  Returns the medium capacity.
  * @param  lun: Logical unit number
  * @param  block_num: Number of total block number
  * @param  block_size: Block size
  * @retval Status (0: OK / -1: Error)
  */
int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t * block_num,
                           uint16_t * block_size)
{
    HAL_SD_CardInfoTypeDef info;
    int8_t ret = -1;

    if (BSP_SD_IsDetected(0) != SD_NOT_PRESENT) {
        BSP_SD_GetCardInfo(0, &info);

        *block_num = info.LogBlockNbr - 1;
        *block_size = info.LogBlockSize;
        ret = 0;
    }
    return ret;
}

/**
  * @brief  Checks whether the medium is ready.
  * @param  lun: Logical unit number
  * @retval Status (0: OK / -1: Error)
  */
int8_t STORAGE_IsReady(uint8_t lun)
{
    static int8_t prev_status = 0;
    int8_t ret = -1;

    if (BSP_SD_IsDetected(0) != SD_NOT_PRESENT) {
        if (prev_status < 0) {
            BSP_SD_Init(0);
            prev_status = 0;

        }

        if (BSP_SD_GetCardState(0) == SD_TRANSFER_OK) {
            ret = 0;
        }
    } else if (prev_status == 0) {
        prev_status = -1;
    }
    return ret;
}

/**
  * @brief  Checks whether the medium is write protected.
  * @param  lun: Logical unit number
  * @retval Status (0: write enabled / -1: otherwise)
  */
int8_t STORAGE_IsWriteProtected(uint8_t lun)
{
    return 0;
}

/**
  * @brief  Reads data from the medium.
  * @param  lun: Logical unit number
  * @param  blk_addr: Logical block address
  * @param  blk_len: Blocks number
  * @retval Status (0: OK / -1: Error)
  */
#define SD_TIMEOUT 100
// 32 byte aligned buffer in non cacheable memory for DMA
static uint8_t aligned_buffer[USBD_MSC_BUFFER_SIZE] __attribute__((aligned(32)));
extern volatile int sd_read_ready;
int8_t STORAGE_Read(uint8_t lun, uint8_t * buf, uint32_t blk_addr, uint16_t blk_len)
{
    int8_t ret = -1;
    uint32_t size = blk_len * blocksize;
    if(size > sizeof(aligned_buffer)) {
        printf("MSC Buffer bigger than 512\n");
    }
    if (BSP_SD_IsDetected(0) != SD_NOT_PRESENT) {
        sd_read_ready = 0;
        // make sure cache is flushed and invalidated so when DMA updates the RAM
        // from reading the peripheral the CPU then reads the new data
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)aligned_buffer, size);

        if(BSP_SD_ReadBlocks_DMA(0, (uint32_t*)aligned_buffer, blk_addr, blk_len) == BSP_ERROR_NONE) {
            // Wait for read to complete or a timeout
            uint32_t timeout = HAL_GetTick();
            while((sd_read_ready == 0) && ((HAL_GetTick() - timeout) < SD_TIMEOUT)) {}
            if (sd_read_ready != 0) {
                sd_read_ready = 0;
                memcpy(buf, aligned_buffer, size);
                timeout = HAL_GetTick();
                while((HAL_GetTick() - timeout) < SD_TIMEOUT) {
                    if (BSP_SD_GetCardState(0) == SD_TRANSFER_OK) {
                        // invalidate again just in case some other thread touched the cache
                        SCB_InvalidateDCache_by_Addr((uint32_t*)aligned_buffer, size);
                        break;
                    }
                }
                ret = 0;
            } else {
                // incase of a timeout return error
                ret = -2;
            }
        }
    }
    return ret;
}

/**
  * @brief  Writes data into the medium.
  * @param  lun: Logical unit number
  * @param  blk_addr: Logical block address
  * @param  blk_len: Blocks number
  * @retval Status (0 : OK / -1 : Error)
  */
extern volatile int sd_write_ready;
int8_t STORAGE_Write(uint8_t lun, uint8_t * buf, uint32_t blk_addr, uint16_t blk_len)
{
    int8_t ret = -1;
    uint32_t size = blk_len * blocksize;
    if (BSP_SD_IsDetected(0) != SD_NOT_PRESENT) {
        sd_write_ready = 0;
        memcpy(aligned_buffer, buf, size);
        // make sure cache is flushed to RAM so the DMA can read the correct data
        SCB_CleanDCache_by_Addr((uint32_t*)aligned_buffer, size);
        if(BSP_SD_WriteBlocks_DMA(0, (uint32_t *) aligned_buffer, blk_addr, blk_len) == BSP_ERROR_NONE) {
           // Wait for write to complete or a timeout
            uint32_t timeout = HAL_GetTick();
            while((sd_write_ready == 0) && ((HAL_GetTick() - timeout) < SD_TIMEOUT)) {}
            if (sd_write_ready != 0) {
                sd_write_ready = 0;
                timeout = HAL_GetTick();
                while((HAL_GetTick() - timeout) < SD_TIMEOUT) {
                    if (BSP_SD_GetCardState(0) == SD_TRANSFER_OK) {
                        break;
                    }
                }
                ret = 0;
            } else {
                // incase of a timeout return error
                ret = -2;
            }
        }
    }
    return ret;
}

/**
  * @brief  Returns the Max Supported LUNs.
  * @param  None
  * @retval Lun(s) number
  */
int8_t STORAGE_GetMaxLun(void)
{
    return 1;
}


// msc interface


USBD_ReturnType Read_dev (uint8_t lun, uint8_t *dest, uint32_t blockAddr, uint16_t blockLen) /*!< Read media block */
{
    // Do read operation
    int ret = STORAGE_Read(lun, dest, blockAddr, blockLen);
    return ret == 0 ? USBD_E_OK : USBD_E_ERROR;
}

USBD_ReturnType Write_dev (uint8_t lun, uint8_t *src, uint32_t blockAddr, uint16_t blockLen) /*!< Write media block */
{
    // Do write operation
    int ret = STORAGE_Write(lun, src, blockAddr, blockLen);
    return ret == 0 ? USBD_E_OK : USBD_E_ERROR;
}

void Init_dev(uint8_t lun)
{
    // Initialize...

    uint32_t block_num;

    int ret = STORAGE_GetCapacity(lun, &block_num, &blocksize);
    if(ret == 0) {
        Status_dev.BlockCount = block_num;
        Status_dev.BlockSize = blocksize;
    }

    msc_if->LUs[lun].Status->Ready = 1;
    msc_if->LUs[lun].Status->Writable = 1;
}

void DeInit_dev(uint8_t lun)
{
}
