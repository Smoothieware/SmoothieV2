/*
 * handles interrupt driven uart I/O for primary UART/DEBUG port
 */
#include "uart_comms.h"

#include "stm32h7xx_hal.h"

#include <stdlib.h>
#include <string.h>

static xTaskHandle xTaskToNotify = NULL;

/* Transmit and receive buffers */
static uint8_t rxbuff[16];
static uint16_t rxbuffsize= 0;
static UART_HandleTypeDef UartHandle;

// select the UART to use
#if defined(USE_UART3) && UART3_PINSET == 8

/* UART3 on nucleo is routed to the stlinkv3 */
#define USARTx                     USART3
#define USARTx_CLK_ENABLE()             __HAL_RCC_USART3_CLK_ENABLE()
#define USARTx_CLK_DISABLE()            __HAL_RCC_USART3_CLK_DISABLE()

#define USARTx_TX_PIN                   GPIO_PIN_8
#define USARTx_TX_GPIO_PORT             GPIOD
#define USARTx_TX_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOD_CLK_ENABLE()
#define USARTx_TX_GPIO_CLK_DISABLE()    __HAL_RCC_GPIOD_CLK_DISABLE()
#define USARTx_TX_AF                    GPIO_AF7_USART3

#define USARTx_RX_PIN                   GPIO_PIN_9
#define USARTx_RX_GPIO_PORT             GPIOD
#define USARTx_RX_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOD_CLK_ENABLE()
#define USARTx_RX_GPIO_CLK_DISABLE()    __HAL_RCC_GPIOD_CLK_DISABLE()
#define USARTx_RX_AF                    GPIO_AF7_USART3

#define USARTx_FORCE_RESET()             __HAL_RCC_USART3_FORCE_RESET()
#define USARTx_RELEASE_RESET()           __HAL_RCC_USART3_RELEASE_RESET()

/* Definition for USARTx's NVIC */
#define USARTx_IRQn                      USART3_IRQn
#define USARTx_IRQHandler                USART3_IRQHandler

#else
#error Board needs to define which UART to use (USE_UART[0|1|2|3])
#endif

void USARTx_IRQHandler(void)
{
	HAL_UART_IRQHandler(&UartHandle);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(xTaskToNotify != NULL) {
		// we only do this if there is new incoming data
		// notify task there is data to read
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t nb_rx_data)
{
	// we got a partial buffer
	rxbuffsize= nb_rx_data;

}

void set_notification_uart(xTaskHandle h)
{
	/* Store the handle of the calling task. */
	xTaskToNotify = h;
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
	if(huart != &UartHandle) {
		// not this one
		return;
	}

	GPIO_InitTypeDef  GPIO_InitStruct;

	/*##-1- Enable peripherals and GPIO Clocks #################################*/
	/* Enable GPIO TX/RX clock */
	USARTx_TX_GPIO_CLK_ENABLE();
	USARTx_RX_GPIO_CLK_ENABLE();

	/* Enable USARTx clock */
	USARTx_CLK_ENABLE();

	/*##-2- Configure peripheral GPIO ##########################################*/
	/* UART TX GPIO pin configuration  */
	GPIO_InitStruct.Pin       = USARTx_TX_PIN;
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_PULLUP;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = USARTx_TX_AF;

	HAL_GPIO_Init(USARTx_TX_GPIO_PORT, &GPIO_InitStruct);

	/* UART RX GPIO pin configuration  */
	GPIO_InitStruct.Pin = USARTx_RX_PIN;
	GPIO_InitStruct.Alternate = USARTx_RX_AF;

	HAL_GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStruct);

	/*##-3- Configure the NVIC for UART ########################################*/
	/* NVIC for USART */
	HAL_NVIC_SetPriority(USARTx_IRQn, 0, 1);
	HAL_NVIC_EnableIRQ(USARTx_IRQn);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
	if(huart != &UartHandle) {
		// not this one
		return;
	}

	/*##-1- Reset peripherals ##################################################*/
	USARTx_FORCE_RESET();
	USARTx_RELEASE_RESET();

	/*##-2- Disable peripherals and GPIO Clocks #################################*/
	/* Configure UART Tx as alternate function  */
	HAL_GPIO_DeInit(USARTx_TX_GPIO_PORT, USARTx_TX_PIN);
	/* Configure UART Rx as alternate function  */
	HAL_GPIO_DeInit(USARTx_RX_GPIO_PORT, USARTx_RX_PIN);

	/*##-3- Disable the NVIC for UART ##########################################*/
	HAL_NVIC_DisableIRQ(USARTx_IRQn);
}

int setup_uart()
{
	/*##-1- Configure the UART peripheral ######################################*/
	/* Put the USART peripheral in the Asynchronous mode (UART Mode) */
	/* UART configured as follows:
	    - Word Length = 8 Bits
	    - Stop Bit = One Stop bit
	    - Parity = None
	    - BaudRate = 115200 baud
	    - Hardware flow control disabled (RTS and CTS signals) */
	UartHandle.Instance        = USARTx;
	UartHandle.Init.BaudRate   = 115200;
	UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
	UartHandle.Init.StopBits   = UART_STOPBITS_1;
	UartHandle.Init.Parity     = UART_PARITY_NONE;
	UartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
	UartHandle.Init.Mode       = UART_MODE_TX_RX;
	UartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if(HAL_UART_DeInit(&UartHandle) != HAL_OK) {
		return 0;
	}
	if(HAL_UART_Init(&UartHandle) != HAL_OK) {
		return 0;
	}

	// HAL_UART_EnableReceiverTimeout(&UartHandle);

	/*##-4- Put UART peripheral in reception process ###########################*/
	if(HAL_UART_Receive_IT(&UartHandle, (uint8_t *)rxbuff, 16) != HAL_OK) {
		return 0;
	}


	return 1;
}

void stop_uart()
{
	HAL_UART_MspDeInit(&UartHandle);
}

size_t read_uart(char * buf, size_t length)
{
	if(rxbuffsize > 1) {
		uint16_t n= rxbuffsize > length ? length : rxbuffsize;
		rxbuffsize= 0;

		memcpy(buf, rxbuff, n);
		return n;

	}else{
		*buf = *rxbuff;
		rxbuffsize= 0;
		return 1;
	}

}

size_t write_uart(const char *buf, size_t length)
{
#if 0
	// Note we do a blocking write here until all is written
	size_t sent_cnt = 0;
	while(sent_cnt < length) {
		int n = Chip_UART_SendRB(LPC_UARTX, &txring, buf + sent_cnt, length - sent_cnt);
		if(n > 0) {
			sent_cnt += n;
		}
	}
	return length;
#else
	HAL_UART_Transmit(&UartHandle, (uint8_t*)buf, length, 5000);
	return length;
#endif
}
