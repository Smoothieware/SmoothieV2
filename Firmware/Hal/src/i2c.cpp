#include "i2c.h"

#include "Hal_pin.h"

#include "stm32h7xx_hal.h"
#ifdef USE_FULL_LL_DRIVER
#include "stm32h7xx_ll_rcc.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "event_groups.h"

#define ERR_CPLT 0x01
#define TX_CPLT  0x02
#define RX_CPLT  0x04

I2C *I2C::i2c_channel[2] {nullptr, nullptr};
// static
I2C *I2C::getInstance(int channel)
{
	if(channel >= 2) return nullptr;
	if(i2c_channel[channel] == nullptr) {
		i2c_channel[channel] = new I2C(channel);
	}

	return i2c_channel[channel];
}
void I2C::deleteInstance(int channel)
{
	if(channel >= 2) return;
	if(i2c_channel[channel] == nullptr) {
		return;
	}

	delete i2c_channel[channel];
	i2c_channel[channel] = nullptr;
}

I2C::I2C(int ch)
{
	channel = ch;
	_valid = false;
}

I2C::~I2C()
{
	if(_valid) {
		HAL_I2C_DeInit((I2C_HandleTypeDef*)_hi2c);
		free(_hi2c);
		vEventGroupDelete( (EventGroupHandle_t)_peventg);
	}
}

bool I2C::read(uint8_t slave_addr, uint8_t *buf, int len)
{
	// Read data
	if(HAL_I2C_Master_Receive_IT((I2C_HandleTypeDef*)_hi2c, (uint16_t)slave_addr, (uint8_t *)buf, len) != HAL_OK) {
		return false;
	}

	// wait for event telling us it has completed (or got an error)
	EventBits_t uxBits = xEventGroupWaitBits(_peventg, TX_CPLT | ERR_CPLT, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));

	if(uxBits & RX_CPLT) {
		// should be ready if we got the interrupt
		while (HAL_I2C_GetState((I2C_HandleTypeDef*)_hi2c) != HAL_I2C_STATE_READY) {
		}
		return true;

	} else if(uxBits & ERR_CPLT) {
		return false;

	} else {
		// timed out
		return false;
	}

	return true;
}

bool I2C::write(uint8_t slave_addr, uint8_t *buf, int len)
{
	// Send data
	do { // maybe resend if we did not get an ack
		if(HAL_I2C_Master_Transmit_IT((I2C_HandleTypeDef*)_hi2c, (uint16_t)slave_addr, (uint8_t*)buf, len) != HAL_OK) {
			return false;
		}

		// wait for event telling us it was sent ok
		EventBits_t uxBits = xEventGroupWaitBits(_peventg, TX_CPLT | ERR_CPLT, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));

		if(uxBits & TX_CPLT) {
			// should be ready if we got the interrupt
			while (HAL_I2C_GetState((I2C_HandleTypeDef*)_hi2c) != HAL_I2C_STATE_READY) {
			}
			return true;

		} else if(uxBits & ERR_CPLT) {
			continue;

		} else {
			// timed out
			return false;
		}

		// When Acknowledge failure occurs (Slave don't acknowledge it's address) Master restarts communication */
	} while(HAL_I2C_GetError((I2C_HandleTypeDef*)_hi2c) == HAL_I2C_ERROR_AF);

	return false;
}

void I2C::set_event(uint8_t ev)
{
	// sets the event to notify the waiting task
	BaseType_t xHigherPriorityTaskWoken, xResult;
	xHigherPriorityTaskWoken = pdFALSE;
	xResult = xEventGroupSetBitsFromISR(_peventg, ev, &xHigherPriorityTaskWoken);
	if( xResult != pdFAIL ) {
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/*
I2C2_SCL        PF1
I2C2_SDA        PF0
I2C3_SCL        PA8
I2C3_SDA        PH8
*/

// Set channel 0 to I2C2
#define I2Cx1                            I2C2
#define I2Cx1_CLK_ENABLE()               __HAL_RCC_I2C2_CLK_ENABLE()
#define I2Cx1_SDA_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOF_CLK_ENABLE()
#define I2Cx1_SCL_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOF_CLK_ENABLE()

#define I2Cx1_FORCE_RESET()              __HAL_RCC_I2C2_FORCE_RESET()
#define I2Cx1_RELEASE_RESET()            __HAL_RCC_I2C2_RELEASE_RESET()

/* Definition for I2Cx1 Pins */
#define I2Cx1_SCL_PIN                    GPIO_PIN_1
#define I2Cx1_SCL_GPIO_PORT              GPIOF
#define I2Cx1_SDA_PIN                    GPIO_PIN_0
#define I2Cx1_SDA_GPIO_PORT              GPIOF
#define I2Cx1_SCL_SDA_AF                 GPIO_AF4_I2C2

/* Definition for I2Cx1's NVIC */
#define I2Cx1_EV_IRQn                    I2C2_EV_IRQn
#define I2Cx1_ER_IRQn                    I2C2_ER_IRQn
#define I2Cx1_EV_IRQHandler              I2C2_EV_IRQHandler
#define I2Cx1_ER_IRQHandler              I2C2_ER_IRQHandler

// Set channel 1 to I2C3
#define I2Cx2                            I2C3
#define I2Cx2_CLK_ENABLE()               __HAL_RCC_I2C3_CLK_ENABLE()
#define I2Cx2_SDA_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOH_CLK_ENABLE()
#define I2Cx2_SCL_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOA_CLK_ENABLE()

#define I2Cx2_FORCE_RESET()              __HAL_RCC_I2C3_FORCE_RESET()
#define I2Cx2_RELEASE_RESET()            __HAL_RCC_I2C3_RELEASE_RESET()

/* Definition for I2Cx2 Pins */
#define I2Cx2_SCL_PIN                    GPIO_PIN_8
#define I2Cx2_SCL_GPIO_PORT              GPIOA
#define I2Cx2_SDA_PIN                    GPIO_PIN_8
#define I2Cx2_SDA_GPIO_PORT              GPIOH
#define I2Cx2_SCL_SDA_AF                 GPIO_AF4_I2C3

/* Definition for I2Cx2's NVIC */
#define I2Cx2_EV_IRQn                    I2C3_EV_IRQn
#define I2Cx2_ER_IRQn                    I2C3_ER_IRQn
#define I2Cx2_EV_IRQHandler              I2C3_EV_IRQHandler
#define I2Cx2_ER_IRQHandler              I2C3_ER_IRQHandler


bool I2C::init(int frequency)
{
	if(_valid) {
		if(frequency != _frequency) {
			printf("ERROR: I2C channel %d, already set with different frequency\n", channel);
			return false;
		}
		return true;
	}

#ifdef USE_FULL_LL_DRIVER
	uint32_t i2c_hz = LL_RCC_GetI2CClockFreq(LL_RCC_I2C123_CLKSOURCE);
	printf("DEBUG: I2C clk source: %lu hz\n", i2c_hz);
	//uint32_t f = (uint32_t) (i2c_hz / _frequency);
#else
	// divisor needed for requested frequency
	//uint32_t f = (uint32_t) (SystemCoreClock / (2 * _frequency));
#endif
	// TIMING Rise time = 100 ns, Fall time = 10 ns
	// we are currently ignoring the frequency becuase STM decided you can only calculate this using the CubeMX tool!!!
	// however we could lookup the closest value below derived from CubeMX
	// 0x00E01a2f for 1Mhz
	// 0x20601138 for 400Khz
	// 0x40604e73 for 100Khz
	// 0x40608aFF for 50Khz
	// This example use TIMING to 0x10810915 to reach 1 MHz speed (Rise time = 120 ns, Fall time = 100 ns)

	uint32_t i2c_timing = 0x40604e73; // 0x00901954; // TODO figure out how to calculate this

	/*##-1- Configure the I2C peripheral ######################################*/
	I2C_HandleTypeDef I2cHandle{0};
	I2cHandle.Instance             = (channel == 0) ? I2Cx1 : I2Cx2;
	I2cHandle.Init.Timing          = i2c_timing;
	I2cHandle.Init.OwnAddress1     = 0;
	I2cHandle.Init.AddressingMode  = I2C_ADDRESSINGMODE_10BIT;
	I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	I2cHandle.Init.OwnAddress2     = 0;
	I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	I2cHandle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

	if(HAL_I2C_Init(&I2cHandle) != HAL_OK) {
		return false;
	}

	/* Enable the Analog I2C Filter */
	HAL_I2CEx_ConfigAnalogFilter(&I2cHandle, I2C_ANALOGFILTER_ENABLE);

	_hi2c = malloc(sizeof(I2C_HandleTypeDef));
	memcpy(_hi2c, &I2cHandle, sizeof(I2C_HandleTypeDef));
	_frequency = frequency;
	_valid = true;

	// event group to be used to notify of completion
	_peventg = xEventGroupCreate();


	return true;
}

/**
  * @brief I2C MSP Initialization
  *        This function configures the hardware resources used in this example:
  *           - Peripheral's clock enable
  *           - Peripheral's GPIO Configuration
  *           - DMA configuration for transmission request by peripheral
  *           - NVIC configuration for DMA interrupt request enable
  * @param hi2c: I2C handle pointer
  * @retval None
  */
extern "C" void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
	GPIO_InitTypeDef  GPIO_InitStruct;

	if(hi2c->Instance == I2Cx1) {
		/*##-1- Enable peripherals and GPIO Clocks #################################*/
		/* Enable GPIO TX/RX clock */
		I2Cx1_SCL_GPIO_CLK_ENABLE();
		I2Cx1_SDA_GPIO_CLK_ENABLE();
		/* Enable I2Cx clock */
		I2Cx1_CLK_ENABLE();

		/*##-2- Configure peripheral GPIO ##########################################*/
		/* I2C TX GPIO pin configuration  */
		GPIO_InitStruct.Pin       = I2Cx1_SCL_PIN;
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull      = GPIO_PULLUP;
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_InitStruct.Alternate = I2Cx1_SCL_SDA_AF;
		HAL_GPIO_Init(I2Cx1_SCL_GPIO_PORT, &GPIO_InitStruct);

		/* I2C RX GPIO pin configuration  */
		GPIO_InitStruct.Pin       = I2Cx1_SDA_PIN;
		GPIO_InitStruct.Alternate = I2Cx1_SCL_SDA_AF;
		HAL_GPIO_Init(I2Cx1_SDA_GPIO_PORT, &GPIO_InitStruct);

		/*##-3- Configure the NVIC for I2C ########################################*/
		/* NVIC for I2Cx */
		NVIC_SetPriority(I2Cx1_ER_IRQn, 5);
		NVIC_EnableIRQ(I2Cx1_ER_IRQn);
		NVIC_SetPriority(I2Cx1_EV_IRQn, 5);
		NVIC_EnableIRQ(I2Cx1_EV_IRQn);

	} else if(hi2c->Instance == I2Cx2) {
		I2Cx2_SCL_GPIO_CLK_ENABLE();
		I2Cx2_SDA_GPIO_CLK_ENABLE();
		I2Cx2_CLK_ENABLE();
		GPIO_InitStruct.Pin       = I2Cx2_SCL_PIN;
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull      = GPIO_PULLUP;
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
		GPIO_InitStruct.Alternate = I2Cx2_SCL_SDA_AF;
		HAL_GPIO_Init(I2Cx2_SCL_GPIO_PORT, &GPIO_InitStruct);
		GPIO_InitStruct.Pin       = I2Cx2_SDA_PIN;
		GPIO_InitStruct.Alternate = I2Cx2_SCL_SDA_AF;
		HAL_GPIO_Init(I2Cx2_SDA_GPIO_PORT, &GPIO_InitStruct);
		NVIC_SetPriority(I2Cx2_ER_IRQn, 5);
		NVIC_EnableIRQ(I2Cx2_ER_IRQn);
		NVIC_SetPriority(I2Cx2_EV_IRQn, 5);
		NVIC_EnableIRQ(I2Cx2_EV_IRQn);
	}
}

/**
  * @brief I2C MSP De-Initialization
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO, DMA and NVIC configuration to their default state
  * @param hi2c: I2C handle pointer
  * @retval None
  */
extern "C" void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
	if(hi2c->Instance == I2Cx1) {
		/*##-1- Reset peripherals ##################################################*/
		I2Cx1_FORCE_RESET();
		I2Cx1_RELEASE_RESET();

		/*##-2- Disable peripherals and GPIO Clocks #################################*/
		/* Configure I2C Tx as alternate function  */
		HAL_GPIO_DeInit(I2Cx1_SCL_GPIO_PORT, I2Cx1_SCL_PIN);
		/* Configure I2C Rx as alternate function  */
		HAL_GPIO_DeInit(I2Cx1_SDA_GPIO_PORT, I2Cx1_SDA_PIN);

		/*##-3- Disable the NVIC for I2C ##########################################*/
		HAL_NVIC_DisableIRQ(I2Cx1_ER_IRQn);
		HAL_NVIC_DisableIRQ(I2Cx1_EV_IRQn);

	} else if(hi2c->Instance == I2Cx2) {
		I2Cx2_FORCE_RESET();
		I2Cx2_RELEASE_RESET();
		HAL_GPIO_DeInit(I2Cx2_SCL_GPIO_PORT, I2Cx2_SCL_PIN);
		HAL_GPIO_DeInit(I2Cx2_SDA_GPIO_PORT, I2Cx2_SDA_PIN);
		HAL_NVIC_DisableIRQ(I2Cx2_ER_IRQn);
		HAL_NVIC_DisableIRQ(I2Cx2_EV_IRQn);
	}
}

/**
  * @brief  I2C error callbacks.
  * @param  I2cHandle: I2C handle
  * @retval None
  */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *I2cHandle)
{
	I2C *i2c;
	if(I2cHandle->Instance == I2Cx1) {
		i2c = I2C::i2c_channel[0];

	} else if(I2cHandle->Instance == I2Cx2) {
		i2c = I2C::i2c_channel[1];
	}

	// set event to wakeup sender
	i2c->set_event(ERR_CPLT);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
	I2C *i2c;
	if(I2cHandle->Instance == I2Cx1) {
		i2c = I2C::i2c_channel[0];

	} else if(I2cHandle->Instance == I2Cx2) {
		i2c = I2C::i2c_channel[1];
	}

	// set event to wakeup sender
	i2c->set_event(TX_CPLT);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
	I2C *i2c;
	if(I2cHandle->Instance == I2Cx1) {
		i2c = I2C::i2c_channel[0];

	} else if(I2cHandle->Instance == I2Cx2) {
		i2c = I2C::i2c_channel[1];
	}

	// set event to wakeup sender
	i2c->set_event(RX_CPLT);
}

void I2Cx1_EV_IRQHandler(void)
{
	HAL_I2C_EV_IRQHandler((I2C_HandleTypeDef*)I2C::i2c_channel[0]->get_hi2c());
}

/**
  * @brief  This function handles I2C error interrupt request.
  */
void I2Cx1_ER_IRQHandler(void)
{
	HAL_I2C_ER_IRQHandler((I2C_HandleTypeDef*)I2C::i2c_channel[0]->get_hi2c());
}

void I2Cx2_EV_IRQHandler(void)
{
	HAL_I2C_EV_IRQHandler((I2C_HandleTypeDef*)I2C::i2c_channel[1]->get_hi2c());
}

/**
  * @brief  This function handles I2C error interrupt request.
  */
void I2Cx2_ER_IRQHandler(void)
{
	HAL_I2C_ER_IRQHandler((I2C_HandleTypeDef*)I2C::i2c_channel[1]->get_hi2c());
}
