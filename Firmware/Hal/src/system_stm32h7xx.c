/**
  ******************************************************************************
  * @file    FatFs/FatFs_uSD_DMA_RTOS/Src/system_stm32h7xx.c
  * @author  MCD Application Team
  * @brief   CMSIS Cortex-M Device Peripheral Access Layer System Source File.
  *
  *   This file provides two functions and one global variable to be called from
  *   user application:
  *      - SystemInit(): This function is called at startup just after reset and
  *                      before branch to main program. This call is made inside
  *                      the "startup_stm32h7xx.s" file.
  *
  *      - SystemCoreClock variable: Contains the core clock (HCLK), it can be used
  *                                  by the user application to setup the SysTick
  *                                  timer or configure other parameters.
  *
  *      - SystemCoreClockUpdate(): Updates the variable SystemCoreClock and must
  *                                 be called whenever the core clock is changed
  *                                 during program execution.
  *
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/** @addtogroup CMSIS
  * @{
  */

/** @addtogroup stm32h7xx_system
  * @{
  */

/** @addtogroup STM32H7xx_System_Private_Includes
  * @{
  */

#include "stm32h7xx.h"
#include <math.h>
#include <string.h>

#if !defined  (HSE_VALUE)
#define HSE_VALUE    ((uint32_t)8000000) /*!< Value of the External oscillator in Hz */
#endif /* HSE_VALUE */

#if !defined  (CSI_VALUE)
#define CSI_VALUE    ((uint32_t)4000000) /*!< Value of the Internal oscillator in Hz*/
#endif /* CSI_VALUE */

#if !defined  (HSI_VALUE)
#define HSI_VALUE    ((uint32_t)64000000) /*!< Value of the Internal oscillator in Hz*/
#endif /* HSI_VALUE */

/**
  * @}
  */

/** @addtogroup STM32H7xx_System_Private_TypesDefinitions
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32H7xx_System_Private_Defines
  * @{
  */

/************************* Miscellaneous Configuration ************************/
/*!< Uncomment the following line if you need to use external SRAM or SDRAM mounted
     on EVAL board as data memory  */
/*#define DATA_IN_ExtSRAM */
/*#define DATA_IN_ExtSDRAM*/

/*!< Uncomment the following line if you need to use initialized data in D2 domain SRAM  */
#define DATA_IN_D2_SRAM

/*!< Uncomment the following line if you need to relocate your vector Table in
     Internal SRAM. */
/* #define VECT_TAB_SRAM */
#define VECT_TAB_OFFSET  0x00000000UL       /*!< Vector Table base offset field.
                                      This value must be a multiple of 0x200. */
/******************************************************************************/

/**
  * @}
  */

/** @addtogroup STM32H7xx_System_Private_Macros
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32H7xx_System_Private_Variables
  * @{
  */
/* This variable is updated in three ways:
    1) by calling CMSIS function SystemCoreClockUpdate()
    2) by calling HAL API function HAL_RCC_GetHCLKFreq()
    3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
       Note: If you use this function to configure the system clock; then there
             is no need to call the 2 first functions listed above, since SystemCoreClock
             variable is updated automatically.
*/
uint32_t SystemCoreClock = 64000000;
uint32_t SystemD2Clock = 64000000;
const  uint8_t D1CorePrescTable[16] = {0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};

/**
  * @}
  */

/** @addtogroup STM32H7xx_System_Private_FunctionPrototypes
  * @{
  */
#if defined (DATA_IN_ExtSRAM) || defined (DATA_IN_ExtSDRAM)
static void SystemInit_ExtMemCtl(void);
#endif /* DATA_IN_ExtSRAM || DATA_IN_ExtSDRAM */

/**
  * @}
  */

/** @addtogroup STM32H7xx_System_Private_Functions
  * @{
  */

/**
  * @brief  Setup the microcontroller system
  *         Initialize the FPU setting, vector table location and External memory
  *         configuration.
  * @param  None
  * @retval None
  */
void SystemInit (void)
{
#if defined (DATA_IN_D2_SRAM)
	__IO uint32_t tmpreg;
#endif /* DATA_IN_D2_SRAM */

	/* FPU settings ------------------------------------------------------------*/
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
	SCB->CPACR |= ((3UL << (10 * 2)) | (3UL << (11 * 2))); /* set CP10 and CP11 Full Access */
#endif
	/* Reset the RCC clock configuration to the default reset state ------------*/
	/* Set HSION bit */
	RCC->CR |= RCC_CR_HSION;

	/* Reset CFGR register */
	RCC->CFGR = 0x00000000;

	/* Reset HSEON, CSSON , CSION,RC48ON, CSIKERON PLL1ON, PLL2ON and PLL3ON bits */
	RCC->CR &= 0xEAF6ED7FU;

	/* Reset D1CFGR register */
	RCC->D1CFGR = 0x00000000;

	/* Reset D2CFGR register */
	RCC->D2CFGR = 0x00000000;

	/* Reset D3CFGR register */
	RCC->D3CFGR = 0x00000000;

	/* Reset PLLCKSELR register */
	RCC->PLLCKSELR = 0x00000000;

	/* Reset PLLCFGR register */
	RCC->PLLCFGR = 0x00000000;
	/* Reset PLL1DIVR register */
	RCC->PLL1DIVR = 0x00000000;
	/* Reset PLL1FRACR register */
	RCC->PLL1FRACR = 0x00000000;

	/* Reset PLL2DIVR register */
	RCC->PLL2DIVR = 0x00000000;

	/* Reset PLL2FRACR register */

	RCC->PLL2FRACR = 0x00000000;
	/* Reset PLL3DIVR register */
	RCC->PLL3DIVR = 0x00000000;

	/* Reset PLL3FRACR register */
	RCC->PLL3FRACR = 0x00000000;

	/* Reset HSEBYP bit */
	RCC->CR &= 0xFFFBFFFFU;

	/* Disable all interrupts */
	RCC->CIER = 0x00000000;

	/* Change  the switch matrix read issuing capability to 1 for the AXI SRAM target (Target 7) */
	if((DBGMCU->IDCODE & 0xFFFF0000U) < 0x20000000U) {
		/* if stm32h7 revY*/
		/* Change  the switch matrix read issuing capability to 1 for the AXI SRAM target (Target 7) */
		*((__IO uint32_t*)0x51008108) = 0x00000001U;
	}

#if defined (DATA_IN_D2_SRAM)
	/* in case of initialized data in D2 SRAM , enable the D2 SRAM clock */
	RCC->AHB2ENR |= (RCC_AHB2ENR_D2SRAM1EN | RCC_AHB2ENR_D2SRAM2EN | RCC_AHB2ENR_D2SRAM3EN);
	tmpreg = RCC->AHB2ENR;
	(void) tmpreg;
#endif /* DATA_IN_D2_SRAM */

	/*
	   * Disable the FMC bank1 (enabled after reset).
	   * This, prevents CPU speculation access on this bank which blocks the use of FMC during
	   * 24us. During this time the others FMC master (such as LTDC) cannot use it!
	   */
	FMC_Bank1_R->BTCR[0] = 0x000030D2;

#if defined (DATA_IN_ExtSRAM) || defined (DATA_IN_ExtSDRAM)
	SystemInit_ExtMemCtl();
#endif /* DATA_IN_ExtSRAM || DATA_IN_ExtSDRAM */


	/* Configure the Vector Table location add offset address ------------------*/
#ifdef VECT_TAB_SRAM
	SCB->VTOR = D1_AXISRAM_BASE  | VECT_TAB_OFFSET;       /* Vector Table Relocation in Internal SRAM */
#else
	SCB->VTOR = FLASH_BANK1_BASE | VECT_TAB_OFFSET;       /* Vector Table Relocation in Internal FLASH */
#endif

	/* Load vector table into ITCM RAM */
	extern unsigned char itcm_vectors_start;
	extern const unsigned char itcm_vectors_end;
	extern const unsigned char __vectors_start__;
	memcpy(&itcm_vectors_start, &__vectors_start__, (uint32_t) (&itcm_vectors_end - &itcm_vectors_start));
	SCB->VTOR = itcm_vectors_start;       /* Vector Table Relocation in Internal SRAM */

	/* Load functions into ITCM RAM */
	extern unsigned char itcm_text_start;
	extern const unsigned char itcm_text_end;
	extern const unsigned char itcm_data;
	memcpy(&itcm_text_start, &itcm_data, (uint32_t) (&itcm_text_end - &itcm_text_start));

	/* Load data into DTCM RAM */
	extern unsigned char dtcm_text_start;
	extern const unsigned char dtcm_text_end;
	extern const unsigned char dtcm_data;
	memcpy(&dtcm_text_start, &dtcm_data, (uint32_t) (&dtcm_text_end - &dtcm_text_start));

	// clear DTCM BSS
	extern unsigned char __dtcm_bss_start, __dtcm_bss_end;
	memset(&__dtcm_bss_start, 0, (uint32_t) (&__dtcm_bss_end - &__dtcm_bss_start));

	// clear SRAM_1 BSS
	extern unsigned char __sram_1_start, __sram_1_end;
	memset(&__sram_1_start, 0, (uint32_t) (&__sram_1_end - &__sram_1_start));
}

/**
   * @brief  Update SystemCoreClock variable according to Clock Register Values.
  *         The SystemCoreClock variable contains the core clock , it can
  *         be used by the user application to setup the SysTick timer or configure
  *         other parameters.
  *
  * @note   Each time the core clock changes, this function must be called
  *         to update SystemCoreClock variable value. Otherwise, any configuration
  *         based on this variable will be incorrect.
  *
  * @note   - The system frequency computed by this function is not the real
  *           frequency in the chip. It is calculated based on the predefined
  *           constant and the selected clock source:
  *
  *           - If SYSCLK source is CSI, SystemCoreClock will contain the CSI_VALUE(*)
  *           - If SYSCLK source is HSI, SystemCoreClock will contain the HSI_VALUE(**)
  *           - If SYSCLK source is HSE, SystemCoreClock will contain the HSE_VALUE(***)
  *           - If SYSCLK source is PLL, SystemCoreClock will contain the CSI_VALUE(*),
  *             HSI_VALUE(**) or HSE_VALUE(***) multiplied/divided by the PLL factors.
  *
  *         (*) CSI_VALUE is a constant defined in stm32h7xx_hal.h file (default value
  *             4 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.
  *         (**) HSI_VALUE is a constant defined in stm32h7xx_hal.h file (default value
  *             64 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.
  *
  *         (***)HSE_VALUE is a constant defined in stm32h7xx_hal.h file (default value
  *              25 MHz), user has to ensure that HSE_VALUE is same as the real
  *              frequency of the crystal used. Otherwise, this function may
  *              have wrong result.
  *
  *         - The result of this function could be not correct when using fractional
  *           value for HSE crystal.
  * @param  None
  * @retval None
  */
void SystemCoreClockUpdate (void)
{
	uint32_t pllp, pllsource, pllm, pllfracen, hsivalue, tmp;
	float_t fracn1, pllvco;

	/* Get SYSCLK source -------------------------------------------------------*/

	switch (RCC->CFGR & RCC_CFGR_SWS) {
		case RCC_CFGR_SWS_HSI:  /* HSI used as system clock source */
			SystemCoreClock = (uint32_t) (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV) >> 3));

			break;

		case RCC_CFGR_SWS_CSI:  /* CSI used as system clock  source */
			SystemCoreClock = CSI_VALUE;
			break;

		case RCC_CFGR_SWS_HSE:  /* HSE used as system clock  source */
			SystemCoreClock = HSE_VALUE;
			break;

		case RCC_CFGR_SWS_PLL1:  /* PLL1 used as system clock  source */

			/* PLL_VCO = (HSE_VALUE or HSI_VALUE or CSI_VALUE/ PLLM) * PLLN
			SYSCLK = PLL_VCO / PLLR
			*/
			pllsource = (RCC->PLLCKSELR & RCC_PLLCKSELR_PLLSRC);
			pllm = ((RCC->PLLCKSELR & RCC_PLLCKSELR_DIVM1) >> 4)  ;
			pllfracen = ((RCC->PLLCFGR & RCC_PLLCFGR_PLL1FRACEN) >> RCC_PLLCFGR_PLL1FRACEN_Pos);
			fracn1 = (float_t)(uint32_t)(pllfracen * ((RCC->PLL1FRACR & RCC_PLL1FRACR_FRACN1) >> 3));

			if (pllm != 0U) {
				switch (pllsource) {
					case RCC_PLLCKSELR_PLLSRC_HSI:  /* HSI used as PLL clock source */

						hsivalue = (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV) >> 3)) ;
						pllvco = ( (float_t)hsivalue / (float_t)pllm) * ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1 / (float_t)0x2000) + (float_t)1 );

						break;

					case RCC_PLLCKSELR_PLLSRC_CSI:  /* CSI used as PLL clock source */
						pllvco = ((float_t)CSI_VALUE / (float_t)pllm) * ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1 / (float_t)0x2000) + (float_t)1 );
						break;

					case RCC_PLLCKSELR_PLLSRC_HSE:  /* HSE used as PLL clock source */
						pllvco = ((float_t)HSE_VALUE / (float_t)pllm) * ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1 / (float_t)0x2000) + (float_t)1 );
						break;

					default:
						pllvco = ((float_t)CSI_VALUE / (float_t)pllm) * ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1 / (float_t)0x2000) + (float_t)1 );
						break;
				}
				pllp = (((RCC->PLL1DIVR & RCC_PLL1DIVR_P1) >> 9) + 1U ) ;
				SystemCoreClock =  (uint32_t)(float_t)(pllvco / (float_t)pllp);
			} else {
				SystemCoreClock = 0U;
			}
			break;

		default:
			SystemCoreClock = CSI_VALUE;
			break;
	}

	/* Compute SystemClock frequency --------------------------------------------------*/

	tmp = D1CorePrescTable[(RCC->D1CFGR & RCC_D1CFGR_D1CPRE) >> RCC_D1CFGR_D1CPRE_Pos];
	/* SystemCoreClock frequency : CM7 CPU frequency  */
	SystemCoreClock >>= tmp;

	/* SystemD2Clock frequency : AXI and AHBs Clock frequency  */
	SystemD2Clock = (SystemCoreClock >> ((D1CorePrescTable[(RCC->D1CFGR & RCC_D1CFGR_HPRE) >> RCC_D1CFGR_HPRE_Pos]) & 0x1FU));
}
#if defined (DATA_IN_ExtSRAM) || defined (DATA_IN_ExtSDRAM)
/**
  * @brief  Setup the external memory controller.
  *         Called in startup_stm32h7xx.s before jump to main.
  *         This function configures the external memories (SRAM/SDRAM)
  *         This SRAM/SDRAM will be used as program data memory (including heap and stack).
  * @param  None
  * @retval None
  */
void SystemInit_ExtMemCtl(void)
{
	__IO uint32_t tmp = 0;

#if defined (DATA_IN_ExtSDRAM) && defined (DATA_IN_ExtSRAM)
	register uint32_t tmpreg = 0, timeout = 0xFFFF;
	register __IO uint32_t index;

	/* Enable GPIOD, GPIOE, GPIOF, GPIOG, GPIOH and GPIOI interface
	    clock */
	RCC->AHB4ENR |= 0x000001F8;

	/* Delay after an RCC peripheral clock enabling */
	tmp = READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOEEN);

	/* Connect PDx pins to FMC Alternate function */
	GPIOD->AFR[0]  = 0x00CC00CC;
	GPIOD->AFR[1]  = 0xCCCCCCCC;
	/* Configure PDx pins in Alternate function mode */
	GPIOD->MODER   = 0xAAAAFAFA;
	/* Configure PDx pins speed to 100 MHz */
	GPIOD->OSPEEDR = 0xFFFF0F0F;
	/* Configure PDx pins Output type to push-pull */
	GPIOD->OTYPER  = 0x00000000;
	/* Configure PDx pins in Pull-up */
	GPIOD->PUPDR   = 0x55550505;

	/* Connect PEx pins to FMC Alternate function */
	GPIOE->AFR[0]  = 0xC00CC0CC;
	GPIOE->AFR[1]  = 0xCCCCCCCC;
	/* Configure PEx pins in Alternate function mode */
	GPIOE->MODER   = 0xAAAABEBA;
	/* Configure PEx pins speed to 100 MHz */
	GPIOE->OSPEEDR = 0xFFFFC3CF;
	/* Configure PEx pins Output type to push-pull */
	GPIOE->OTYPER  = 0x00000000;
	/* Configure PEx pins in Pull-up */
	GPIOE->PUPDR   = 0x55554145;

	/* Connect PFx pins to FMC Alternate function */
	GPIOF->AFR[0]  = 0x00CCCCCC;
	GPIOF->AFR[1]  = 0xCCCCC000;
	/* Configure PFx pins in Alternate function mode */
	GPIOF->MODER   = 0xAABFFAAA;
	/* Configure PFx pins speed to 100 MHz */
	GPIOF->OSPEEDR = 0xFFC00FFF;
	/* Configure PFx pins Output type to push-pull */
	GPIOF->OTYPER  = 0x00000000;
	/* Configure PFx pins in Pull-up */
	GPIOF->PUPDR   = 0x55400555;

	/* Connect PGx pins to FMC Alternate function */
	GPIOG->AFR[0]  = 0x00CCCCCC;
	GPIOG->AFR[1]  = 0xC0000C0C;
	/* Configure PGx pins in Alternate function mode */
	GPIOG->MODER   = 0xBFEEFAAA;
	/* Configure PGx pins speed to 100 MHz */
	GPIOG->OSPEEDR = 0xC0330FFF;
	/* Configure PGx pins Output type to push-pull */
	GPIOG->OTYPER  = 0x00000000;
	/* Configure PGx pins in Pull-up */
	GPIOG->PUPDR   = 0x40110555;

	/* Connect PHx pins to FMC Alternate function */
	GPIOH->AFR[0]  = 0xCCC00000;
	GPIOH->AFR[1]  = 0xCCCCCCCC;
	/* Configure PHx pins in Alternate function mode */
	GPIOH->MODER   = 0xAAAAABFF;
	/* Configure PHx pins speed to 100 MHz */
	GPIOH->OSPEEDR = 0xFFFFFC00;
	/* Configure PHx pins Output type to push-pull */
	GPIOH->OTYPER  = 0x00000000;
	/* Configure PHx pins in Pull-up */
	GPIOH->PUPDR   = 0x55555400;

	/* Connect PIx pins to FMC Alternate function */
	GPIOI->AFR[0]  = 0xCCCCCCCC;
	GPIOI->AFR[1]  = 0x00000CC0;
	/* Configure PIx pins in Alternate function mode */
	GPIOI->MODER   = 0xFFEBAAAA;
	/* Configure PIx pins speed to 100 MHz */
	GPIOI->OSPEEDR = 0x003CFFFF;
	/* Configure PIx pins Output type to push-pull */
	GPIOI->OTYPER  = 0x00000000;
	/* Configure PIx pins in Pull-up */
	GPIOI->PUPDR   = 0x00145555;

	/* Enable the FMC/FSMC interface clock */
	(RCC->AHB3ENR |= (RCC_AHB3ENR_FMCEN));

	/* Configure and enable Bank1_SRAM2 */
	FMC_Bank1_R->BTCR[4]  = 0x00001091;
	FMC_Bank1_R->BTCR[5]  = 0x00110212;
	FMC_Bank1E_R->BWTR[4] = 0x0FFFFFFF;

	/*SDRAM Timing and access interface configuration*/
	/*LoadToActiveDelay  = 2
	  ExitSelfRefreshDelay = 6
	  SelfRefreshTime      = 4
	  RowCycleDelay        = 6
	  WriteRecoveryTime    = 2
	  RPDelay              = 2
	  RCDDelay             = 2
	  SDBank             = FMC_SDRAM_BANK2
	  ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_9
	  RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_12
	  MemoryDataWidth    = FMC_SDRAM_MEM_BUS_WIDTH_32
	  InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4
	  CASLatency         = FMC_SDRAM_CAS_LATENCY_2
	  WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE
	  SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2
	  ReadBurst          = FMC_SDRAM_RBURST_ENABLE
	  ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0*/

	FMC_Bank5_6_R->SDCR[0] = 0x00001800;
	FMC_Bank5_6_R->SDCR[1] = 0x00000165;
	FMC_Bank5_6_R->SDTR[0] = 0x00105000;
	FMC_Bank5_6_R->SDTR[1] = 0x01010351;

	/* SDRAM initialization sequence */
	/* Clock enable command */
	FMC_Bank5_6_R->SDCMR = 0x00000009;
	tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}

	/* Delay */
	for (index = 0; index < 1000; index++);

	/* PALL command */
	FMC_Bank5_6_R->SDCMR = 0x0000000A;
	timeout = 0xFFFF;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}

	FMC_Bank5_6_R->SDCMR = 0x000000EB;
	timeout = 0xFFFF;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}

	FMC_Bank5_6_R->SDCMR = 0x0004400C;
	timeout = 0xFFFF;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}
	/* Set refresh count */
	tmpreg = FMC_Bank5_6_R->SDRTR;
	FMC_Bank5_6_R->SDRTR = (tmpreg | (0x00000603 << 1));

	/* Disable write protection */
	tmpreg = FMC_Bank5_6_R->SDCR[1];
	FMC_Bank5_6_R->SDCR[1] = (tmpreg & 0xFFFFFDFF);

	/*FMC controller Enable*/
	FMC_Bank1_R->BTCR[0]  |= 0x80000000;

#elif defined (DATA_IN_ExtSDRAM)
	register uint32_t tmpreg = 0, timeout = 0xFFFF;
	register __IO uint32_t index;

	/* Enable GPIOD, GPIOE, GPIOF, GPIOG, GPIOH and GPIOI interface
	    clock */
	RCC->AHB4ENR |= 0x000001F8;

	/* Connect PDx pins to FMC Alternate function */
	GPIOD->AFR[0]  = 0x000000CC;
	GPIOD->AFR[1]  = 0xCC000CCC;
	/* Configure PDx pins in Alternate function mode */
	GPIOD->MODER   = 0xAFEAFFFA;
	/* Configure PDx pins speed to 100 MHz */
	GPIOD->OSPEEDR = 0xF03F000F;
	/* Configure PDx pins Output type to push-pull */
	GPIOD->OTYPER  = 0x00000000;
	/* Configure PDx pins in Pull-up */
	GPIOD->PUPDR   = 0x50150005;

	/* Connect PEx pins to FMC Alternate function */
	GPIOE->AFR[0]  = 0xC00000CC;
	GPIOE->AFR[1]  = 0xCCCCCCCC;
	/* Configure PEx pins in Alternate function mode */
	GPIOE->MODER   = 0xAAAABFFA;
	/* Configure PEx pins speed to 100 MHz */
	GPIOE->OSPEEDR = 0xFFFFC00F;
	/* Configure PEx pins Output type to push-pull */
	GPIOE->OTYPER  = 0x00000000;
	/* Configure PEx pins in Pull-up */
	GPIOE->PUPDR   = 0x55554005;

	/* Connect PFx pins to FMC Alternate function */
	GPIOF->AFR[0]  = 0x00CCCCCC;
	GPIOF->AFR[1]  = 0xCCCCC000;
	/* Configure PFx pins in Alternate function mode */
	GPIOF->MODER   = 0xAABFFAAA;
	/* Configure PFx pins speed to 100 MHz */
	GPIOF->OSPEEDR = 0xFFC00FFF;
	/* Configure PFx pins Output type to push-pull */
	GPIOF->OTYPER  = 0x00000000;
	/* Configure PFx pins in Pull-up */
	GPIOF->PUPDR   = 0x55400555;

	/* Connect PGx pins to FMC Alternate function */
	GPIOG->AFR[0]  = 0x00CCCCCC;
	GPIOG->AFR[1]  = 0xC000000C;
	/* Configure PGx pins in Alternate function mode */
	GPIOG->MODER   = 0xBFFEFAAA;
	/* Configure PGx pins speed to 100 MHz */
	GPIOG->OSPEEDR = 0xC0030FFF;
	/* Configure PGx pins Output type to push-pull */
	GPIOG->OTYPER  = 0x00000000;
	/* Configure PGx pins in Pull-up */
	GPIOG->PUPDR   = 0x40010555;

	/* Connect PHx pins to FMC Alternate function */
	GPIOH->AFR[0]  = 0xCCC00000;
	GPIOH->AFR[1]  = 0xCCCCCCCC;
	/* Configure PHx pins in Alternate function mode */
	GPIOH->MODER   = 0xAAAAABFF;
	/* Configure PHx pins speed to 100 MHz */
	GPIOH->OSPEEDR = 0xFFFFFC00;
	/* Configure PHx pins Output type to push-pull */
	GPIOH->OTYPER  = 0x00000000;
	/* Configure PHx pins in Pull-up */
	GPIOH->PUPDR   = 0x55555400;

	/* Connect PIx pins to FMC Alternate function */
	GPIOI->AFR[0]  = 0xCCCCCCCC;
	GPIOI->AFR[1]  = 0x00000CC0;
	/* Configure PIx pins in Alternate function mode */
	GPIOI->MODER   = 0xFFEBAAAA;
	/* Configure PIx pins speed to 100 MHz */
	GPIOI->OSPEEDR = 0x003CFFFF;
	/* Configure PIx pins Output type to push-pull */
	GPIOI->OTYPER  = 0x00000000;
	/* Configure PIx pins in Pull-up */
	GPIOI->PUPDR   = 0x00145555;

	/*-- FMC Configuration ------------------------------------------------------*/
	/* Enable the FMC interface clock */
	(RCC->AHB3ENR |= (RCC_AHB3ENR_FMCEN));
	/*SDRAM Timing and access interface configuration*/
	/*LoadToActiveDelay  = 2
	  ExitSelfRefreshDelay = 6
	  SelfRefreshTime      = 4
	  RowCycleDelay        = 6
	  WriteRecoveryTime    = 2
	  RPDelay              = 2
	  RCDDelay             = 2
	  SDBank             = FMC_SDRAM_BANK2
	  ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_9
	  RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_12
	  MemoryDataWidth    = FMC_SDRAM_MEM_BUS_WIDTH_32
	  InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4
	  CASLatency         = FMC_SDRAM_CAS_LATENCY_2
	  WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE
	  SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2
	  ReadBurst          = FMC_SDRAM_RBURST_ENABLE
	  ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0*/

	FMC_Bank5_6_R->SDCR[0] = 0x00001800;
	FMC_Bank5_6_R->SDCR[1] = 0x00000165;
	FMC_Bank5_6_R->SDTR[0] = 0x00105000;
	FMC_Bank5_6_R->SDTR[1] = 0x01010351;

	/* SDRAM initialization sequence */
	/* Clock enable command */
	FMC_Bank5_6_R->SDCMR = 0x00000009;
	tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}

	/* Delay */
	for (index = 0; index < 1000; index++);

	/* PALL command */
	FMC_Bank5_6_R->SDCMR = 0x0000000A;
	timeout = 0xFFFF;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}

	FMC_Bank5_6_R->SDCMR = 0x000000EB;
	timeout = 0xFFFF;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}

	FMC_Bank5_6_R->SDCMR = 0x0004400C;
	timeout = 0xFFFF;
	while((tmpreg != 0) && (timeout-- > 0)) {
		tmpreg = FMC_Bank5_6_R->SDSR & 0x00000020;
	}
	/* Set refresh count */
	tmpreg = FMC_Bank5_6_R->SDRTR;
	FMC_Bank5_6_R->SDRTR = (tmpreg | (0x00000603 << 1));

	/* Disable write protection */
	tmpreg = FMC_Bank5_6_R->SDCR[1];
	FMC_Bank5_6_R->SDCR[1] = (tmpreg & 0xFFFFFDFF);

	/*FMC controller Enable*/
	FMC_Bank1_R->BTCR[0]  |= 0x80000000;

#elif defined(DATA_IN_ExtSRAM)
	/*-- GPIOs Configuration -----------------------------------------------------*/
	/* Enable GPIOD, GPIOE, GPIOF and GPIOG interface clock */
	RCC->AHB4ENR   |= 0x00000078;

	/* Connect PDx pins to FMC Alternate function */
	GPIOD->AFR[0]  = 0x00CC00CC;
	GPIOD->AFR[1]  = 0xCCCCCCCC;
	/* Configure PDx pins in Alternate function mode */
	GPIOD->MODER   = 0xAAAAFABA;
	/* Configure PDx pins speed to 100 MHz */
	GPIOD->OSPEEDR = 0xFFFF0F0F;
	/* Configure PDx pins Output type to push-pull */
	GPIOD->OTYPER  = 0x00000000;
	/* Configure PDx pins in Pull-up */
	GPIOD->PUPDR   = 0x55550505;

	/* Connect PEx pins to FMC Alternate function */
	GPIOE->AFR[0]  = 0xC00CC0CC;
	GPIOE->AFR[1]  = 0xCCCCCCCC;
	/* Configure PEx pins in Alternate function mode */
	GPIOE->MODER   = 0xAAAABEBA;
	/* Configure PEx pins speed to 100 MHz */
	GPIOE->OSPEEDR = 0xFFFFC3CF;
	/* Configure PEx pins Output type to push-pull */
	GPIOE->OTYPER  = 0x00000000;
	/* Configure PEx pins in Pull-up */
	GPIOE->PUPDR   = 0x55554145;

	/* Connect PFx pins to FMC Alternate function */
	GPIOF->AFR[0]  = 0x00CCCCCC;
	GPIOF->AFR[1]  = 0xCCCC0000;
	/* Configure PFx pins in Alternate function mode */
	GPIOF->MODER   = 0xAAFFFAAA;
	/* Configure PFx pins speed to 100 MHz */
	GPIOF->OSPEEDR = 0xFF000FFF;
	/* Configure PFx pins Output type to push-pull */
	GPIOF->OTYPER  = 0x00000000;
	/* Configure PFx pins in Pull-up */
	GPIOF->PUPDR   = 0x55000555;

	/* Connect PGx pins to FMC Alternate function */
	GPIOG->AFR[0]  = 0x00CCCCCC;
	GPIOG->AFR[1]  = 0x00000C00;
	/* Configure PGx pins in Alternate function mode */
	GPIOG->MODER   = 0xFFEFFAAA;
	/* Configure PGx pins speed to 100 MHz */
	GPIOG->OSPEEDR = 0x00300FFF;
	/* Configure PGx pins Output type to push-pull */
	GPIOG->OTYPER  = 0x00000000;
	/* Configure PGx pins in Pull-up */
	GPIOG->PUPDR   = 0x00100555;

	/*-- FMC/FSMC Configuration --------------------------------------------------*/
	/* Enable the FMC/FSMC interface clock */
	(RCC->AHB3ENR |= (RCC_AHB3ENR_FMCEN));

	/* Configure and enable Bank1_SRAM2 */
	FMC_Bank1_R->BTCR[4]  = 0x00001091;
	FMC_Bank1_R->BTCR[5]  = 0x00110212;
	FMC_Bank1E_R->BWTR[4] = 0x0FFFFFFF;

	/*FMC controller Enable*/
	FMC_Bank1_R->BTCR[0]  |= 0x80000000;

#endif /* DATA_IN_ExtSRAM */

	(void)(tmp);
}
#endif /* DATA_IN_ExtSRAM || DATA_IN_ExtSDRAM */


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

void Error_Handler()
{
	while(1) {}
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
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	// Supply configuration update enable
#ifdef STM32H745xx
	HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
#else
	HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
#endif
	/** Configure the main internal regulator output voltage
	*/
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
	/** Macro to configure the PLL clock source
	*/
	__HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
#ifdef BOARD_NUCLEO
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
#else
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
#endif
	RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
	RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

#ifdef BOARD_NUCLEO
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 100;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
#else
	RCC_OscInitStruct.PLL.PLLM = 5;
	RCC_OscInitStruct.PLL.PLLN = 160;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
#endif

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
	/* PLL3 for USB Clock */
#ifdef BOARD_NUCLEO
	PeriphClkInitStruct.PLL3.PLL3M = 1;
	PeriphClkInitStruct.PLL3.PLL3N = 24;
	PeriphClkInitStruct.PLL3.PLL3P = 2;
	PeriphClkInitStruct.PLL3.PLL3R = 2;
	PeriphClkInitStruct.PLL3.PLL3Q = 4;
	PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOMEDIUM;
	PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
#else
	PeriphClkInitStruct.PLL3.PLL3M = 25;
	PeriphClkInitStruct.PLL3.PLL3N = 336;
	PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
	PeriphClkInitStruct.PLL3.PLL3P = 2;
	PeriphClkInitStruct.PLL3.PLL3R = 2;
	PeriphClkInitStruct.PLL3.PLL3Q = 7;
	PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOMEDIUM;
	PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
#endif

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
	PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL3;
	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
	                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
	                              | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
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

/**
  * @brief  Configure the MPU attributes as Write Through for Internal SRAM1.
  * @note   The Base Address 0x24000000 is the SRAM1 accessible by the SDIO internal DMA.
  *         The Configured Region Size is 512KB because same as SRAM1 size.
  * @param  None
  * @retval None
  * NOTE the ERRATA for thi schip says write through can corrupt data so we cannot use it
  */
#define CACHE_WRITE_THROUGH
static void MPU_Config(void)
{
	MPU_Region_InitTypeDef MPU_InitStruct;

	/* Disable the MPU */
	HAL_MPU_Disable();

	/* Configure the MPU attributes as Normal Non Cacheable for SRAM_2 */
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress = 0x30020000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
	MPU_InitStruct.Number = MPU_REGION_NUMBER1;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);


	/* Enable the MPU */
	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

// called from main, but it is HAL so we put it here
void main_system_setup()
{
	MPU_Config();
	// Enable the CPU Cache
	CPU_CACHE_Enable();
	// stm32h7xx HAL library initialization
	if(HAL_Init() != HAL_OK) {
		Error_Handler();
	}
	// Configure the system clock to 400 MHz
	SystemClock_Config();
}

#include <stdio.h>
void print_clocks()
{
	printf("HCLK = %lu\n", HAL_RCC_GetHCLKFreq());
	printf("PCLK1 = %lu\n", HAL_RCC_GetPCLK1Freq());
	printf("PCLK2 = %lu\n", HAL_RCC_GetPCLK2Freq());
}

__attribute__( ( naked, noreturn ) ) void BootJumpASM( uint32_t SP, uint32_t RH )
{
  __asm("MSR      MSP,r0");
  __asm("BX       r1");
}

extern void TIM6_Deinit();
void jump_to_program(uint32_t prog_addr)
{
	// transfer control to the flashloader at prog_addr
	volatile uint32_t *BootAddr= (uint32_t *)prog_addr;

	if(CONTROL_nPRIV_Msk & __get_CONTROL( )) {
		printf("WARNING: Not in priv mode\n");
	}

	/* Set the clock to the default state */
	// HAL_RCC_DeInit();

	/* Disable all interrupts */
	__disable_irq();
	/* Disable Systick timer */
	SysTick->CTRL = 0;
	/* disable Hal tick timer */
	TIM6_Deinit();

	// make sure everything has written through
	__DSB();
	__ISB();

	// Clear All Interrupt Enable Registers & Interrupt Pending Registers
	// we do it twice in case we get an interrupt while we do this
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 5; i++) {
			NVIC->ICER[i] = 0xFFFFFFFF;
			NVIC->ICPR[i] = 0xFFFFFFFF;
		}
		__DSB();
		__ISB();
	}

	// disable fault handling
	SCB->SHCSR &= ~( SCB_SHCSR_USGFAULTENA_Msk | \
	                 SCB_SHCSR_BUSFAULTENA_Msk | \
	                 SCB_SHCSR_MEMFAULTENA_Msk ) ;

	if( CONTROL_SPSEL_Msk & __get_CONTROL( ) ) {
		/* MSP is not active */
		__set_MSP( __get_PSP( ) ) ;
		__set_CONTROL( __get_CONTROL( ) & ~CONTROL_SPSEL_Msk ) ;
	}

	/* Re-enable all interrupts */
	__enable_irq();

	SCB->VTOR = prog_addr;       // Vector Table Relocation

	BootJumpASM(BootAddr[0], BootAddr[1]) ;
}

#if defined(DUAL_CORE)
#define RST_FLAG RCC_FLAG_SFTR1ST
#define CLEAR_RESET() __HAL_RCC_C1_CLEAR_RESET_FLAGS()
#else
#define RST_FLAG RCC_FLAG_SFTRST
#define CLEAR_RESET() __HAL_RCC_CLEAR_RESET_FLAGS()
#endif

void check_last_reset_status()
{
#if 0
	if(__HAL_RCC_GET_FLAG(RST_FLAG) == 1) {
		CLEAR_RESET();
		SCB_DisableICache();
		SCB_DisableDCache();

		// last reset was a soft reset, so check magic in ITCMRAM and see if we
		// need to run the flashloader from ITCMRAM
		extern uint8_t _binary_flashloader_bin_size[];
		size_t data_size  = (size_t)_binary_flashloader_bin_size;
		uint64_t *data_end = (uint64_t*)(0x00000000 + data_size);
		data_end -= 1; // point to last 64 bits
		if(*data_end == 0x1234567898765432LL) { // magic at the end of the flashloader
			*data_end = 0; // clear it so we won't try running here again
			jump_to_program(0);
			while(1) ;
		}
	}
#endif
	// other reset or no magic number so continue as normal
}

