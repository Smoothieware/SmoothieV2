/**
  ******************************************************************************
  * @file    USB_Device/DFU_Standalone/Src/usbd_dfu_flash.c
  * @author  MCD Application Team
  * @brief   Memory management layer
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

/* Includes ------------------------------------------------------------------ */
#include "stm32h7xx_hal.h"
#include "usbd_dfu.h"

/* Private typedef ----------------------------------------------------------- */
/* Private define ------------------------------------------------------------ */
#define FLASH_DESC_STR      "@Internal Flash   /0x08000000/15*128Kg"
#define FLASH_ERASE_TIME    (uint16_t)0
#define FLASH_PROGRAM_TIME  (uint16_t)0

/* Base address of the Flash sectors Bank 1 */
#define ADDR_FLASH_SECTOR_0_BANK1     ((uint32_t)0x08000000) /* Base @ of Sector 0, 128 Kbytes */
#define ADDR_FLASH_SECTOR_1_BANK1     ((uint32_t)0x08020000) /* Base @ of Sector 1, 128 Kbytes */
#define ADDR_FLASH_SECTOR_2_BANK1     ((uint32_t)0x08040000) /* Base @ of Sector 2, 128 Kbytes */
#define ADDR_FLASH_SECTOR_3_BANK1     ((uint32_t)0x08060000) /* Base @ of Sector 3, 128 Kbytes */
#define ADDR_FLASH_SECTOR_4_BANK1     ((uint32_t)0x08080000) /* Base @ of Sector 4, 128 Kbytes */
#define ADDR_FLASH_SECTOR_5_BANK1     ((uint32_t)0x080A0000) /* Base @ of Sector 5, 128 Kbytes */
#define ADDR_FLASH_SECTOR_6_BANK1     ((uint32_t)0x080C0000) /* Base @ of Sector 6, 128 Kbytes */
#define ADDR_FLASH_SECTOR_7_BANK1     ((uint32_t)0x080E0000) /* Base @ of Sector 7, 128 Kbytes */

/* Base address of the Flash sectors Bank 2 */
#define ADDR_FLASH_SECTOR_0_BANK2     ((uint32_t)0x08100000) /* Base @ of Sector 0, 128 Kbytes */
#define ADDR_FLASH_SECTOR_1_BANK2     ((uint32_t)0x08120000) /* Base @ of Sector 1, 128 Kbytes */
#define ADDR_FLASH_SECTOR_2_BANK2     ((uint32_t)0x08140000) /* Base @ of Sector 2, 128 Kbytes */
#define ADDR_FLASH_SECTOR_3_BANK2     ((uint32_t)0x08160000) /* Base @ of Sector 3, 128 Kbytes */
#define ADDR_FLASH_SECTOR_4_BANK2     ((uint32_t)0x08180000) /* Base @ of Sector 4, 128 Kbytes */
#define ADDR_FLASH_SECTOR_5_BANK2     ((uint32_t)0x081A0000) /* Base @ of Sector 5, 128 Kbytes */
#define ADDR_FLASH_SECTOR_6_BANK2     ((uint32_t)0x081C0000) /* Base @ of Sector 6, 128 Kbytes */
#define ADDR_FLASH_SECTOR_7_BANK2     ((uint32_t)0x081E0000) /* Base @ of Sector 7, 128 Kbytes */

#define FLASH_BASE_ADDR      (uint32_t)(FLASH_BASE)
#define FLASH_END_ADDR       (uint32_t)(0x081FFFFF)

#define DFU_MEDIA_ERASE                0x00U
#define DFU_MEDIA_PROGRAM              0x01U

typedef struct
{
  const uint8_t *pStrDesc;
  uint16_t (* Init)(void);
  uint16_t (* DeInit)(void);
  uint16_t (* Erase)(uint32_t Add);
  uint16_t (* Write)(uint8_t *src, uint8_t *dest, uint32_t Len);
  uint8_t *(* Read)(uint8_t *src, uint8_t *dest, uint32_t Len);
  uint16_t (* GetStatus)(uint32_t Add, uint8_t cmd, uint8_t *buff);
} USBD_DFU_MediaTypeDef;

/* Private macro ------------------------------------------------------------- */
/* Private variables --------------------------------------------------------- */
/* Private function prototypes ----------------------------------------------- */
static uint32_t GetSector(uint32_t Address);

/* Extern function prototypes ------------------------------------------------ */
uint16_t Flash_If_Init(void);
uint16_t Flash_If_Erase(uint32_t Add);
uint16_t Flash_If_Write(uint8_t * src, uint8_t * dest, uint32_t Len);
uint8_t *Flash_If_Read(uint8_t * src, uint8_t * dest, uint32_t Len);
uint16_t Flash_If_DeInit(void);
uint16_t Flash_If_GetStatus(uint32_t Add, uint8_t Cmd, uint8_t * buffer);

// wrappers to map calls
static USBD_DFU_StatusType FlashIf_Manifest(void)
{
	return DFU_ERROR_NONE;
}

static uint16_t FlashIf_GetTimeout_ms(uint8_t *addr, uint32_t len)
{
	// 32 bytes takes about 500us
	//return (500*(len/32))/1000;
	// As we wait for each flash to finish there is no timeout value needed
	// we could interleave then a timeout would be as above.
	return 0;
}

static uint8_t erased_sector[8];
static void FlashIf_Init()
{
	Flash_If_Init();
	for (int i = 0; i < sizeof(erased_sector); ++i) {
	    erased_sector[i]= 0;
	}
}

static void FlashIf_Deinit()
{
	Flash_If_DeInit();
}

static USBD_DFU_StatusType FlashIf_Erase(uint8_t *addr)
{
	// erase 128K, mark it as erased
	int sector= GetSector((uint32_t)addr);
	if(erased_sector[sector] == 0) {
		if(Flash_If_Erase((uint32_t)addr) != 0) {
			return DFU_ERROR_ERASE;
		}
		erased_sector[sector]= 1;
	}
	return DFU_ERROR_NONE;
}

static USBD_DFU_StatusType FlashIf_Write(uint8_t *addr, uint8_t *data, uint32_t len)
{
	if(Flash_If_Write(data, addr, len) != 0) {
		return DFU_ERROR_WRITE;
	}
	return DFU_ERROR_NONE;
}

static void FlashIf_Read(uint8_t *addr, uint8_t *data, uint32_t len)
{
	Flash_If_Read(data, addr, len);
}

const USBD_DFU_AppType hflash_if = {
	#ifdef DEBUG
    .Firmware.Address   = ADDR_FLASH_SECTOR_1_BANK1,
    #else
    .Firmware.Address   = FLASH_BASE_ADDR,
    #endif
    .Firmware.TotalSize = 0x00200000 - 0x00020000, // 2MB less last 128K

    .Init               = FlashIf_Init,
    .Deinit             = FlashIf_Deinit,
    .Write              = FlashIf_Write,
    .Read               = FlashIf_Read,
    .Manifest           = FlashIf_Manifest,

#if (USBD_DFU_ST_EXTENSION != 0)
    .Erase              = SE_FlashIf_Erase,
    .GetTimeout_ms      = SE_FlashIf_GetTimeout_ms,
    .Name               = SE_FLASH_DESC_STR,
#else
    .Erase              = FlashIf_Erase,
    .GetTimeout_ms      = FlashIf_GetTimeout_ms,
    .Name               = "DFU Bootloader",
#endif
}, *const flash_if = &hflash_if;


/* Private functions --------------------------------------------------------- */
/**
  * @brief  Initializes Memory.
  * @param  None
  * @retval 0 if operation is successful, MAL_FAIL else.
  */
uint16_t Flash_If_Init(void)
{
  /* Unlock the internal flash */
  HAL_FLASH_Unlock();
  return 0;
}

/**
  * @brief  De-Initializes Memory.
  * @param  None
  * @retval 0 if operation is successful, MAL_FAIL else.
  */
uint16_t Flash_If_DeInit(void)
{
  /* Lock the internal flash */
  HAL_FLASH_Lock();
  return 0;
}

/**
  * @brief  Erases sector. 128K bytes
  * @param  Add: Address of sector to be erased.
  * @retval 0 if operation is successful, MAL_FAIL else.
  */
uint16_t Flash_If_Erase(uint32_t Add)
{
  uint32_t startsector = 0, sectorerror = 0;

  /* Variable contains Flash operation status */
  HAL_StatusTypeDef status;
  FLASH_EraseInitTypeDef eraseinitstruct;

  /* Get the number of sector */
  startsector = GetSector(Add);
  eraseinitstruct.TypeErase = FLASH_TYPEERASE_SECTORS;
  eraseinitstruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  eraseinitstruct.Banks = (Add >= ADDR_FLASH_SECTOR_0_BANK2) ? FLASH_BANK_2 : FLASH_BANK_1;
  eraseinitstruct.Sector = startsector;
  eraseinitstruct.NbSectors = 1;

  status = HAL_FLASHEx_Erase(&eraseinitstruct, &sectorerror);

  if (status != HAL_OK)
  {
    return 1;
  }
  return 0;
}

/**
  * @brief  Writes Data into Memory.
  * @param  src: Pointer to the source buffer. Address to be written to.
  * @param  dest: Pointer to the destination buffer.
  * @param  Len: Number of data to be written (in bytes).
  * @retval 0 if operation is successful, MAL_FAIL else.
  */

uint16_t Flash_If_Write(uint8_t * src, uint8_t * dest, uint32_t Len)
{
  uint32_t i = 0;

  for (i = 0; i < Len; i += 32)
  {
    /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
     * be done by byte */
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, (uint32_t) (dest + i), (uint32_t) (src + i)) == HAL_OK)
    {
      /* Check the written value */
      if (*(uint64_t *) (src + i) != *(uint64_t *) (dest + i))
      {
        /* Flash content doesn't match SRAM content */
        return 2;
      }
    }
    else
    {
      /* Error occurred while writing data in Flash memory */
      return 1;
    }
  }
  return 0;
}

/**
  * @brief  Reads Data into Memory.
  * @param  src: Pointer to the source buffer. Address to be written to.
  * @param  dest: Pointer to the destination buffer.
  * @param  Len: Number of data to be read (in bytes).
  * @retval Pointer to the physical address where data should be read.
  */
uint8_t *Flash_If_Read(uint8_t * src, uint8_t * dest, uint32_t Len)
{
  uint32_t i = 0;
  uint8_t *psrc = src;

  for (i = 0; i < Len; i++)
  {
    dest[i] = *psrc++;
  }
  /* Return a valid address to avoid HardFault */
  return (uint8_t *) (dest);
}

/**
  * @brief  Gets Memory Status.
  * @param  Add: Address to be read from.
  * @param  Cmd: Number of data to be read (in bytes).
  * @retval 0 if operation is successful
  */
uint16_t Flash_If_GetStatus(uint32_t Add, uint8_t Cmd, uint8_t * buffer)
{
  switch (Cmd)
  {
  case DFU_MEDIA_PROGRAM:
    buffer[1] = (uint8_t) FLASH_PROGRAM_TIME;
    buffer[2] = (uint8_t) (FLASH_PROGRAM_TIME << 8);
    buffer[3] = 0;
    break;

  case DFU_MEDIA_ERASE:
  default:
    buffer[1] = (uint8_t) FLASH_ERASE_TIME;
    buffer[2] = (uint8_t) (FLASH_ERASE_TIME << 8);
    buffer[3] = 0;
    break;
  }
  return 0;
}

/**
  * @brief  Gets the sector of a given address
  * @param  Address Address of the FLASH Memory
  * @retval The sector of a given address
  */
static uint32_t GetSector(uint32_t Address)
{
  uint32_t sector = 0;

  if (((Address < ADDR_FLASH_SECTOR_1_BANK1) &&
       (Address >= ADDR_FLASH_SECTOR_0_BANK1)) ||
      ((Address < ADDR_FLASH_SECTOR_1_BANK2) &&
       (Address >= ADDR_FLASH_SECTOR_0_BANK2)))
  {
    sector = FLASH_SECTOR_0;
  }
  else
    if (((Address < ADDR_FLASH_SECTOR_2_BANK1) &&
         (Address >= ADDR_FLASH_SECTOR_1_BANK1)) ||
        ((Address < ADDR_FLASH_SECTOR_2_BANK2) &&
         (Address >= ADDR_FLASH_SECTOR_1_BANK2)))
  {
    sector = FLASH_SECTOR_1;
  }
  else
    if (((Address < ADDR_FLASH_SECTOR_3_BANK1) &&
         (Address >= ADDR_FLASH_SECTOR_2_BANK1)) ||
        ((Address < ADDR_FLASH_SECTOR_3_BANK2) &&
         (Address >= ADDR_FLASH_SECTOR_2_BANK2)))
  {
    sector = FLASH_SECTOR_2;
  }
  else
    if (((Address < ADDR_FLASH_SECTOR_4_BANK1) &&
         (Address >= ADDR_FLASH_SECTOR_3_BANK1)) ||
        ((Address < ADDR_FLASH_SECTOR_4_BANK2) &&
         (Address >= ADDR_FLASH_SECTOR_3_BANK2)))
  {
    sector = FLASH_SECTOR_3;
  }
  else
    if (((Address < ADDR_FLASH_SECTOR_5_BANK1) &&
         (Address >= ADDR_FLASH_SECTOR_4_BANK1)) ||
        ((Address < ADDR_FLASH_SECTOR_5_BANK2) &&
         (Address >= ADDR_FLASH_SECTOR_4_BANK2)))
  {
    sector = FLASH_SECTOR_4;
  }
  else
    if (((Address < ADDR_FLASH_SECTOR_6_BANK1) &&
         (Address >= ADDR_FLASH_SECTOR_5_BANK1)) ||
        ((Address < ADDR_FLASH_SECTOR_6_BANK2) &&
         (Address >= ADDR_FLASH_SECTOR_5_BANK2)))
  {
    sector = FLASH_SECTOR_5;
  }
  else
    if (((Address < ADDR_FLASH_SECTOR_7_BANK1) &&
         (Address >= ADDR_FLASH_SECTOR_6_BANK1)) ||
        ((Address < ADDR_FLASH_SECTOR_7_BANK2) &&
         (Address >= ADDR_FLASH_SECTOR_6_BANK2)))
  {
    sector = FLASH_SECTOR_6;
  }
  else
    if (((Address < ADDR_FLASH_SECTOR_0_BANK2) &&
         (Address >= ADDR_FLASH_SECTOR_7_BANK1)) || ((Address < FLASH_END_ADDR)
                                                     && (Address >=
                                                         ADDR_FLASH_SECTOR_7_BANK2)))
  {
    sector = FLASH_SECTOR_7;
  }
  else
  {
    sector = FLASH_SECTOR_7;
  }

  return sector;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
