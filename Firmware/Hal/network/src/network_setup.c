#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "Hal_pin.h"
#include "FreeRTOSIPConfig.h"

#include <stdlib.h>

int setup_network_hal()
{
	return 1;
}

void inet_addr_convert(uint32_t addr, uint8_t uc_octet[4])
{
#if ipconfigBYTE_ORDER == pdFREERTOS_LITTLE_ENDIAN
	uc_octet[0] = (uint8_t)(addr & 0xFF);
	uc_octet[1]	= (uint8_t)(addr >> 8);
	uc_octet[2]	= (uint8_t)(addr >> 16);
	uc_octet[3]	= (uint8_t)(addr >> 24);
#else
	uc_octet[3] = (uint8_t)(addr & 0xFF);
	uc_octet[2]	= (uint8_t)(addr >> 8);
	uc_octet[1]	= (uint8_t)(addr >> 16);
	uc_octet[0]	= (uint8_t)(addr >> 24);
#endif
}

BaseType_t xApplicationGetRandomNumber( uint32_t *pulNumber )
{
	*pulNumber = rand();
	return pdTRUE;
}

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
        uint16_t usSourcePort,
        uint32_t ulDestinationAddress,
        uint16_t usDestinationPort )
{
	return rand();
}

/**
* @brief ETH MSP Initialization
* This function configures the hardware resources used in this example
* @param heth: ETH handle pointer
* @retval None
*/
#ifdef BOARD_PRIME
void HAL_ETH_MspInit(ETH_HandleTypeDef* heth)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if(heth->Instance == ETH) {
		/* USER CODE BEGIN ETH_MspInit 0 */

		/* USER CODE END ETH_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_ETH1MAC_CLK_ENABLE();
		__HAL_RCC_ETH1TX_CLK_ENABLE();
		__HAL_RCC_ETH1RX_CLK_ENABLE();

		__HAL_RCC_GPIOC_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();
		__HAL_RCC_GPIOG_CLK_ENABLE();

		/**ETH GPIO Configuration
		PA1     ------> ETH_REF_CLK
		PA2     ------> ETH_MDIO
		PA7     ------> ETH_CRS_DV
		PB11    ------> ETH_TX_EN
		PB12    ------> ETH_TXD0
		PC1     ------> ETH_MDC
		PC4     ------> ETH_RXD0
		PC5     ------> ETH_RXD1
		PG14    ------> ETH_TXD1
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_14;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

		/* USER CODE BEGIN ETH_MspInit 1 */
		/* ETH interrupt Init */
		NVIC_SetPriority(ETH_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
		NVIC_EnableIRQ(ETH_IRQn);

		allocate_hal_pin(GPIOA, GPIO_PIN_1);
		allocate_hal_pin(GPIOA, GPIO_PIN_2);
		allocate_hal_pin(GPIOA, GPIO_PIN_7);
		allocate_hal_pin(GPIOB, GPIO_PIN_11);
		allocate_hal_pin(GPIOB, GPIO_PIN_12);
		allocate_hal_pin(GPIOC, GPIO_PIN_1);
		allocate_hal_pin(GPIOC, GPIO_PIN_4);
		allocate_hal_pin(GPIOC, GPIO_PIN_5);
		allocate_hal_pin(GPIOG, GPIO_PIN_14);

		/* USER CODE END ETH_MspInit 1 */
	}
}

/**
* @brief ETH MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param heth: ETH handle pointer
* @retval None
*/
void HAL_ETH_MspDeInit(ETH_HandleTypeDef* heth)
{
	if(heth->Instance == ETH) {
		/* USER CODE BEGIN ETH_MspDeInit 0 */

		/* USER CODE END ETH_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_ETH1MAC_CLK_DISABLE();
		__HAL_RCC_ETH1TX_CLK_DISABLE();
		__HAL_RCC_ETH1RX_CLK_DISABLE();

		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1  | GPIO_PIN_2 | GPIO_PIN_7);
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11 | GPIO_PIN_12);
		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1  | GPIO_PIN_4 | GPIO_PIN_5);
		HAL_GPIO_DeInit(GPIOG, GPIO_PIN_14);

		/* ETH interrupt DeInit */
		HAL_NVIC_DisableIRQ(ETH_IRQn);
		/* USER CODE BEGIN ETH_MspDeInit 1 */

		/* USER CODE END ETH_MspDeInit 1 */
	}
}

#elif defined(BOARD_NUCLEO)
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Ethernett MSP init: RMII Mode */

	/* Enable GPIOs clocks */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();

	/* Ethernet pins configuration ************************************************/
	/*
	      RMII_REF_CLK ----------------------> PA1
	      RMII_MDIO -------------------------> PA2
	      RMII_MII_CRS_DV -------------------> PA7
	      RMII_MII_TXD1 ---------------------> PB13
	      RMII_MDC --------------------------> PC1
	      RMII_MII_RXD0 ---------------------> PC4
	      RMII_MII_RXD1 ---------------------> PC5
	      RMII_MII_TX_EN --------------------> PG11
	      RMII_MII_TXD0 ---------------------> PG13
	*/

	/* Configure PA1, PA2 and PA7 */
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Alternate = GPIO_AF11_ETH;
	GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure PB13 */
	GPIO_InitStructure.Pin = GPIO_PIN_13;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Configure PC1, PC4 and PC5 */
	GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure PG11, PG13  */
	GPIO_InitStructure.Pin =  GPIO_PIN_11 | GPIO_PIN_13;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStructure);

	/* Enable Ethernet clocks */
	__HAL_RCC_ETH1MAC_CLK_ENABLE();
	__HAL_RCC_ETH1TX_CLK_ENABLE();
	__HAL_RCC_ETH1RX_CLK_ENABLE();

	/* ETH interrupt Init */
	NVIC_SetPriority(ETH_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(ETH_IRQn);

	allocate_hal_pin(GPIOA, GPIO_PIN_1);
	allocate_hal_pin(GPIOA, GPIO_PIN_2);
	allocate_hal_pin(GPIOA, GPIO_PIN_7);
	allocate_hal_pin(GPIOB, GPIO_PIN_13);
	allocate_hal_pin(GPIOC, GPIO_PIN_1);
	allocate_hal_pin(GPIOC, GPIO_PIN_4);
	allocate_hal_pin(GPIOC, GPIO_PIN_5);
	allocate_hal_pin(GPIOG, GPIO_PIN_11);
	allocate_hal_pin(GPIOG, GPIO_PIN_13);
}

/**
* @brief ETH MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param heth: ETH handle pointer
* @retval None
*/
void HAL_ETH_MspDeInit(ETH_HandleTypeDef* heth)
{
	if(heth->Instance == ETH) {
		/* USER CODE BEGIN ETH_MspDeInit 0 */

		/* USER CODE END ETH_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_ETH1MAC_CLK_DISABLE();
		__HAL_RCC_ETH1TX_CLK_DISABLE();
		__HAL_RCC_ETH1RX_CLK_DISABLE();

		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1  | GPIO_PIN_2 | GPIO_PIN_7);
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13);
		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1  | GPIO_PIN_4 | GPIO_PIN_5);
		HAL_GPIO_DeInit(GPIOG, GPIO_PIN_11 | GPIO_PIN_13);

		/* ETH interrupt DeInit */
		HAL_NVIC_DisableIRQ(ETH_IRQn);
		/* USER CODE BEGIN ETH_MspDeInit 1 */

		/* USER CODE END ETH_MspDeInit 1 */
	}
}

#endif
