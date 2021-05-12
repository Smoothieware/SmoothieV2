/**
  ******************************************************************************
  * @file    FatFs/FatFs_uSD_Standalone/Src/main.c
  * @author  MCD Application Team
  * @brief   This sample code shows how to use FatFs with uSD card drive.
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
#include "main.h"

#include <string.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
FATFS SDFatFs;  /* File system object for SD card logical drive */
FIL MyFile;     /* File object */
char SDPath[4]; /* SD card logical drive path */

typedef enum {
    APPLICATION_IDLE = 0,
    APPLICATION_RUNNING,
    APPLICATION_SD_UNPLUGGED,
} FS_FileOperationsTypeDef;

FS_FileOperationsTypeDef Appli_state = APPLICATION_IDLE;
/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);
static void FS_FileOperations(void);
static void Error_Handler(void);

// put a magic number at the very end
 __attribute__ ((section (".last_word"))) uint64_t magic=0x1234567898765432LL;

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
    /* Enable the CPU Cache */
    CPU_CACHE_Enable();

    /* stm32h7xx HAL library initialization:
    - Configure the Systick to generate an interrupt each 1 msec
    - Set NVIC Group Priority to 4
    - Global MSP (MCU Support Package) initialization
    */
    HAL_Init();

    /* Configure the system clock to 400 MHz */
    SystemClock_Config();
    extern int setup_uart();
    setup_uart();
    printf("Welcome to Smoothie Boot loader\n");

    /*##-1- Link the micro SD disk I/O driver ##################################*/
    if(FATFS_LinkDriver(&SD_Driver, SDPath) == 0) {
        FS_FileOperations();
    }
    printf("Success!!\n");

    while (1);
}

static FRESULT scan_files (char* pat)
{
    DIR dir;
    FILINFO finfo;
    FATFS *fs;
    FRESULT res = f_opendir(&dir, pat);
    assert(FR_OK == res);
    DWORD p1, s1, s2;
    p1 = s1 = s2 = 0;
    for(;;) {
        res = f_readdir(&dir, &finfo);
        if ((res != FR_OK) || !finfo.fname[0]) break;
        if (finfo.fattrib & AM_DIR) {
            s2++;
        } else {
            s1++; p1 += finfo.fsize;
        }
        printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s\n",
                (finfo.fattrib & AM_DIR) ? 'D' : '-',
                (finfo.fattrib & AM_RDO) ? 'R' : '-',
                (finfo.fattrib & AM_HID) ? 'H' : '-',
                (finfo.fattrib & AM_SYS) ? 'S' : '-',
                (finfo.fattrib & AM_ARC) ? 'A' : '-',
                (finfo.fdate >> 9) + 1980, (finfo.fdate >> 5) & 15, finfo.fdate & 31,
                (finfo.ftime >> 11), (finfo.ftime >> 5) & 63,
                (DWORD)finfo.fsize, finfo.fname);
    }
    printf("%4lu File(s),%10lu bytes total\n%4lu Dir(s)", s1, p1, s2);
    res = f_getfree(pat, (DWORD*)&p1, &fs);
    assert(FR_OK == res);
    printf(", %10lu bytes free\n", p1 * fs->csize * 512);
    f_closedir(&dir);

    return 1;
}

static void FS_FileOperations(void)
{
    FRESULT res;                                          /* FatFs function common result code */
    uint32_t byteswritten, bytesread;                     /* File write/read counts */
    uint8_t wtext[] = "This is STM32 working with FatFs"; /* File write buffer */
    uint8_t rtext[1024];                                   /* File read buffer */

    /* Register the file system object to the FatFs module */
    if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 0) == FR_OK) {
        /* Create and Open a new text file object with write access */
        if(f_open(&MyFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
            /* Write data to the text file */
            res = f_write(&MyFile, wtext, sizeof(wtext), (void *)&byteswritten);

            if((byteswritten > 0) && (res == FR_OK)) {
                /* Close the open text file */
                f_close(&MyFile);

                /* Open the text file object with read access */
                if(f_open(&MyFile, "STM32.TXT", FA_READ) == FR_OK) {
                    /* Read data from the text file */
                    res = f_read(&MyFile, rtext, sizeof(rtext), (void *)&bytesread);

                    if((bytesread > 0) && (res == FR_OK)) {
                        /* Close the open text file */
                        f_close(&MyFile);

                        /* Compare read data with the expected data */
                        if((bytesread == byteswritten)) {
                            scan_files(SDPath);
                            return;
                        }
                    }
                }
            }
        }
    }
    printf("Failed\n");
    /* Error */
    Error_Handler();
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 400000000 (CPU Clock)
  *            HCLK(Hz)                       = 200000000 (AXI and AHBs Clock)
  *            AHB Prescaler                  = 2
  *            D1 APB3 Prescaler              = 2 (APB3 Clock  100MHz)
  *            D2 APB1 Prescaler              = 2 (APB1 Clock  100MHz)
  *            D2 APB2 Prescaler              = 2 (APB2 Clock  100MHz)
  *            D3 APB4 Prescaler              = 2 (APB4 Clock  100MHz)
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 5
  *            PLL_N                          = 160
  *            PLL_P                          = 2
  *            PLL_Q                          = 4
  *            PLL_R                          = 2
  *            VDD(V)                         = 3.3
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    HAL_StatusTypeDef ret = HAL_OK;

    /*!< Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /* The voltage scaling allows optimizing the power consumption when the device is
       clocked below the maximum system frequency, to update the voltage scaling value
       regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
    RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;

    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if(ret != HAL_OK) {
        while(1) { ; }
    }

    /* Select PLL as system clock source and configure  bus clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
                                   RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
    if(ret != HAL_OK) {
        while(1) { ; }
    }

    /*activate CSI clock mondatory for I/O Compensation Cell*/
    __HAL_RCC_CSI_ENABLE() ;

    /* Enable SYSCFG clock mondatory for I/O Compensation Cell */
    __HAL_RCC_SYSCFG_CLK_ENABLE() ;

    /* Enables the I/O Compensation Cell */
    HAL_EnableCompensationCell();
}

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
    /* Enable I-Cache */
    SCB_EnableICache();

    /* Enable D-Cache */
    SCB_EnableDCache();
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

    /* Infinite loop */
    while (1) {
    }
}
#endif

static void Error_Handler(void)
{
    while(1) ;
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
