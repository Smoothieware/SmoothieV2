#include "ff_gen_drv.h"
#include "stm32h7xx_sd.h"

#include <stdio.h>

static int isInitialized = 0;
static char SDPath[5]; /* SD card logical drive path */

extern const Diskio_drvTypeDef SD_Driver;

static int SD_Initialize()
{
	if (isInitialized == 0) {
		uint32_t err = BSP_SD_Init(0);
		if(err != BSP_ERROR_NONE) {
			int serr = (int)err;
			printf("Error: setup_sdmmc - in BSP_SD_Init: %d\n", serr);
			return 0;
		}

		if(BSP_SD_IsDetected(0)) {
			isInitialized = 1;
		}
	}
	return 1;
}

int setup_sdmmc()
{
	// Link the micro SD disk I/O driver
	if(FATFS_LinkDriver(&SD_Driver, SDPath) != 0) {
		printf("Error: setup_sdmmc - link driver failed\n");
		return 0;
	}

	if(SD_Initialize() != 1) {
		printf("Error: setup_sdmmc - SD_Initialize failed\n");
		return 0;
	}

	// if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 1) != FR_OK) {
	// 	printf("Error: setup_sdmmc - mount failed\n");
	// 	return 0;
	// }

	return 1;
}

void shutdown_sdmmc()
{
	BSP_SD_DeInit(0);
}

// NOTE ISR
void BSP_SD_DetectCallback()
{
	// Fixme we need to be able to deinit and re init sdcardf when this happens
	if(BSP_SD_IsDetected(0)) {
		isInitialized = 1;
	} else {
		isInitialized = 0;
	}
}
