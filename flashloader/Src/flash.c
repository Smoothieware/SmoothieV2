#include "stm32h7xx_hal.h"

#define FLASH_BASE_ADDR      (uint32_t)(FLASH_BASE)
#define FLASH_END_ADDR       (uint32_t)(0x081FFFFF)

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


#define FLASH_USER_START_ADDR   ADDR_FLASH_SECTOR_2_BANK1      /* Start @ of user Flash area Bank1 */
#define FLASH_USER_END_ADDR     (ADDR_FLASH_SECTOR_7_BANK1 - 1)  /* End @ of user Flash area Bank1*/

/**
  * @brief  Gets the sector of a given address
  * @param  Address Address of the FLASH Memory
  * @retval The sector of a given address
  */
static uint32_t GetSector(uint32_t Address)
{
	uint32_t sector = 0;

	if(((Address < ADDR_FLASH_SECTOR_1_BANK1) && (Address >= ADDR_FLASH_SECTOR_0_BANK1)) || \
	   ((Address < ADDR_FLASH_SECTOR_1_BANK2) && (Address >= ADDR_FLASH_SECTOR_0_BANK2))) {
		sector = FLASH_SECTOR_0;
	} else if(((Address < ADDR_FLASH_SECTOR_2_BANK1) && (Address >= ADDR_FLASH_SECTOR_1_BANK1)) || \
	          ((Address < ADDR_FLASH_SECTOR_2_BANK2) && (Address >= ADDR_FLASH_SECTOR_1_BANK2))) {
		sector = FLASH_SECTOR_1;
	} else if(((Address < ADDR_FLASH_SECTOR_3_BANK1) && (Address >= ADDR_FLASH_SECTOR_2_BANK1)) || \
	          ((Address < ADDR_FLASH_SECTOR_3_BANK2) && (Address >= ADDR_FLASH_SECTOR_2_BANK2))) {
		sector = FLASH_SECTOR_2;
	} else if(((Address < ADDR_FLASH_SECTOR_4_BANK1) && (Address >= ADDR_FLASH_SECTOR_3_BANK1)) || \
	          ((Address < ADDR_FLASH_SECTOR_4_BANK2) && (Address >= ADDR_FLASH_SECTOR_3_BANK2))) {
		sector = FLASH_SECTOR_3;
	} else if(((Address < ADDR_FLASH_SECTOR_5_BANK1) && (Address >= ADDR_FLASH_SECTOR_4_BANK1)) || \
	          ((Address < ADDR_FLASH_SECTOR_5_BANK2) && (Address >= ADDR_FLASH_SECTOR_4_BANK2))) {
		sector = FLASH_SECTOR_4;
	} else if(((Address < ADDR_FLASH_SECTOR_6_BANK1) && (Address >= ADDR_FLASH_SECTOR_5_BANK1)) || \
	          ((Address < ADDR_FLASH_SECTOR_6_BANK2) && (Address >= ADDR_FLASH_SECTOR_5_BANK2))) {
		sector = FLASH_SECTOR_5;
	} else if(((Address < ADDR_FLASH_SECTOR_7_BANK1) && (Address >= ADDR_FLASH_SECTOR_6_BANK1)) || \
	          ((Address < ADDR_FLASH_SECTOR_7_BANK2) && (Address >= ADDR_FLASH_SECTOR_6_BANK2))) {
		sector = FLASH_SECTOR_6;
	} else if(((Address < ADDR_FLASH_SECTOR_0_BANK2) && (Address >= ADDR_FLASH_SECTOR_7_BANK1)) || \
	          ((Address < FLASH_END_ADDR) && (Address >= ADDR_FLASH_SECTOR_7_BANK2))) {
		sector = FLASH_SECTOR_7;
	} else {
		sector = FLASH_SECTOR_7;
	}

	return sector;
}

int do_flash(void)
{
	uint32_t FirstSector = 0, NbOfSectors = 0;
	uint32_t Address = 0, SECTORError = 0, Index = 0;
	__IO uint32_t MemoryProgramStatus = 0;
	__IO uint64_t data64 = 0;

	uint64_t FlashWord[4] = { 0x0102030405060708,
	                          0x1112131415161718,
	                          0x2122232425262728,
	                          0x3132333435363738
	                        };

	FLASH_EraseInitTypeDef EraseInitStruct;

	HAL_FLASH_Unlock();

	/* -3- Erase the user Flash area
	  (area defined by FLASH_USER_START_ADDR and FLASH_USER_END_ADDR) ***********/

	/* Get the 1st sector to erase */
	FirstSector = GetSector(FLASH_USER_START_ADDR);
	/* Get the number of sector to erase from 1st sector*/
	NbOfSectors = GetSector(FLASH_USER_END_ADDR) - FirstSector + 1;

	/* Fill EraseInit structure*/
	EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
	EraseInitStruct.Banks         = FLASH_BANK_1;
	EraseInitStruct.Sector        = FirstSector;
	EraseInitStruct.NbSectors     = NbOfSectors;

	if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK) {
		/*
		  Error occurred while sector erase.
		  User can add here some code to deal with this error.
		  SECTORError will contain the faulty sector and then to know the code error on this sector,
		  user can call function 'HAL_FLASH_GetError()'
		*/
		/* Infinite loop */
		while (1) {
		}
	}

	/* -4- Program the user Flash area word by word
	  (area defined by FLASH_USER_START_ADDR and FLASH_USER_END_ADDR) ***********/

	Address = FLASH_USER_START_ADDR;

	while (Address < FLASH_USER_END_ADDR) {
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, Address, ((uint32_t)FlashWord)) == HAL_OK) {
			Address = Address + 32; /* increment for the next Flash word*/
		} else {
			/* Error occurred while writing data in Flash memory.
			   User can add here some code to deal with this error */
			while (1) {
			}
		}
	}

	/* -5- Lock the Flash to disable the flash control register access (recommended
	   to protect the FLASH memory against possible unwanted operation) *********/
	HAL_FLASH_Lock();

	/* -6- Check if the programmed data is OK
	    MemoryProgramStatus = 0: data programmed correctly
	    MemoryProgramStatus != 0: number of words not programmed correctly ******/
	Address = FLASH_USER_START_ADDR;
	MemoryProgramStatus = 0x0;

	while (Address < FLASH_USER_END_ADDR) {
		for(Index = 0; Index < 4; Index++) {
			data64 = *(uint64_t*)Address;
			__DSB();
			if(data64 != FlashWord[Index]) {
				MemoryProgramStatus++;
			}
			Address += 8;
		}
	}

	/* -7- Check if there is an issue to program data*/
	if (MemoryProgramStatus == 0) {
		/* No error detected. Switch on LED1*/
	} else {
		/* Error detected. Toggle LED3*/
		while (1) {
		}
	}

	/* Infinite loop */
	while (1) {
	}
}

