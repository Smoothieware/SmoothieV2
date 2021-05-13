#include "main.h"

#include <stdio.h>

extern void Board_LED_Set(uint8_t led, int on);

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

// flash from the file to the Start of FLASH BANK 1
int do_flash(FIL *fp, uint32_t size)
{
	uint32_t FirstSector = 0, NbOfSectors = 0;
	uint32_t Address = 0, SECTORError = 0;

	FLASH_EraseInitTypeDef EraseInitStruct= {0};

	HAL_FLASH_Unlock();

	if(FLASH_BASE_ADDR + size >= ADDR_FLASH_SECTOR_7_BANK1) {
		// erase entire BANK1
		EraseInitStruct.TypeErase     = FLASH_TYPEERASE_MASSERASE;
		EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_4;
		EraseInitStruct.Banks         = FLASH_BANK_1;
		printf("DEBUG: Mass erase of Bank1\n");
	} else {
		// erase as many sectors as needed in bank1
		/* Get the 1st sector to erase */
		FirstSector = GetSector(FLASH_BASE_ADDR);
		/* Get the number of sector to erase from 1st sector*/
		NbOfSectors = GetSector(FLASH_BASE_ADDR + size) - FirstSector + 1;

		/* Fill EraseInit structure*/
		EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
		EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
		EraseInitStruct.Banks         = FLASH_BANK_1;
		EraseInitStruct.Sector        = FirstSector;
		EraseInitStruct.NbSectors     = NbOfSectors;
		printf("DEBUG: Sector erase of Bank1. %lu sectors\n", NbOfSectors);
	}

	if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK) {
		/*
		  Error occurred while sector erase.
		  User can add here some code to deal with this error.
		  SECTORError will contain the faulty sector and then to know the code error on this sector,
		  user can call function 'HAL_FLASH_GetError()'
		*/
		printf("ERROR: erase failed at sector %lu, error %lu\n", SECTORError, HAL_FLASH_GetError());
		return 0;
	}

	// now erase flash bank2 if needed
	if(FLASH_BASE_ADDR + size >= ADDR_FLASH_SECTOR_0_BANK2) {
		// erase as many sectors as needed in bank2
		/* Get the 1st sector to erase */
		FirstSector = GetSector(ADDR_FLASH_SECTOR_0_BANK2);
		/* Get the number of sector to erase from 1st sector*/
		NbOfSectors = GetSector(ADDR_FLASH_SECTOR_0_BANK2 + (size-(ADDR_FLASH_SECTOR_0_BANK2-ADDR_FLASH_SECTOR_0_BANK1))) - FirstSector + 1;

		/* Fill EraseInit structure*/
		EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
		EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
		EraseInitStruct.Banks         = FLASH_BANK_2;
		EraseInitStruct.Sector        = FirstSector;
		EraseInitStruct.NbSectors     = NbOfSectors;
		printf("DEBUG: Sector erase of Bank2. %lu sectors\n", NbOfSectors);

		if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK) {
			return 0;
		}
	}

	printf("DEBUG: starting flash of file\n");

	// Program the user Flash area word by word
	uint8_t cnt= 0;
	uint64_t FlashWord[4];
	UINT br;
	Address = FLASH_BASE_ADDR;
	while (Address < (FLASH_BASE_ADDR + size)) {
		/* Read a chunk of file */
		FRESULT rc = f_read(fp, (void *)FlashWord, sizeof(FlashWord), &br);
		if (rc != FR_OK) {
			return 0; // Error
		}
		if(br == 0) break;

		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, Address, ((uint32_t)FlashWord)) == HAL_OK) {
			//printf("DEBUG: flashed address: %p\n", (void*)Address);
			Address = Address + 32; /* increment for the next Flash word*/

			// count up leds to show progress
			cnt++;
			Board_LED_Set(0, cnt & 1);
			Board_LED_Set(1, cnt & 2);
			Board_LED_Set(2, cnt & 4);
			Board_LED_Set(3, cnt & 8);

		} else {
			printf("ERROR: flash programming error\n");
			return 0;
		}
		if(br < sizeof(FlashWord)) break; // hit EOF
	}

	/* -5- Lock the Flash to disable the flash control register access (recommended
	   to protect the FLASH memory against possible unwanted operation) *********/
	HAL_FLASH_Lock();
	printf("DEBUG: Flash completed\n\n");

#if 0
	/* -6- Check if the programmed data is OK
	    MemoryProgramStatus = 0: data programmed correctly
	    MemoryProgramStatus != 0: number of words not programmed correctly ******/
	__IO uint32_t MemoryProgramStatus = 0;
	__IO uint64_t data64 = 0;
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
		return 1;
	}
#endif
	return 1;
}

