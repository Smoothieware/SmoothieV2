/*
 * handles interrupt driven uart I/O for primary UART/DEBUG port
 */
#include "uart_comms.h"

#include "stm32h7xx_hal.h"

#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_utils.h"
#include "stm32h7xx_ll_gpio.h"
#include "stm32h7xx_ll_usart.h"

#include "ringbuffer_c.h"

#include <stdlib.h>
#include <string.h>

static xTaskHandle xTaskToNotify = NULL;

/* Transmit and receive buffers */
RingBuffer_t *rxrb;

// select the UART to use
#if defined(USE_UART3) && UART3_PINSET == 8

/* UART3 on nucleo is routed to the stlinkv3 */
#define USARTx_INSTANCE               USART3
#define USARTx_CLK_ENABLE()           LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3)
#define USARTx_CLK_SOURCE()           LL_RCC_SetUSARTClockSource(LL_RCC_USART234578_CLKSOURCE_PCLK1)
#define USARTx_IRQn                   USART3_IRQn
#define USARTx_IRQHandler             USART3_IRQHandler

#define USARTx_GPIO_CLK_ENABLE()      LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOD)   /* Enable the peripheral clock of GPIOD */
#define USARTx_TX_PIN                 LL_GPIO_PIN_8
#define USARTx_TX_GPIO_PORT           GPIOD
#define USARTx_SET_TX_GPIO_AF()       LL_GPIO_SetAFPin_8_15(GPIOD, LL_GPIO_PIN_8, LL_GPIO_AF_7)
#define USARTx_RX_PIN                 LL_GPIO_PIN_9
#define USARTx_RX_GPIO_PORT           GPIOD
#define USARTx_SET_RX_GPIO_AF()       LL_GPIO_SetAFPin_8_15(GPIOD, LL_GPIO_PIN_9, LL_GPIO_AF_7)

#else
#error Board needs to define which UART to use (USE_UART[0|1|2|3])
#endif

/**
  * @brief  Function called from USART IRQ Handler when RXNE flag is set
  *         Function is in charge of reading character received on USART RX line.
  * @param  None
  * @retval None
  */
void USART_CharReception_Callback(void)
{
	__IO uint32_t received_char;

	/* Read Received character. RXNE flag is cleared by reading of RDR register */
	received_char = LL_USART_ReceiveData8(USARTx_INSTANCE);

	// stick in circular buffer
	RingBufferPut(rxrb, received_char);

	if(xTaskToNotify != NULL) {
		// we only do this if there is new incoming data
		// notify task there is data to read
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}

void USARTx_IRQHandler(void)
{
	/* Check RXNE flag value in ISR register */
	if(LL_USART_IsActiveFlag_RXNE(USARTx_INSTANCE) && LL_USART_IsEnabledIT_RXNE(USARTx_INSTANCE)) {
		/* RXNE flag will be cleared by reading of RDR register (done in call) */
		/* Call function in charge of handling Character reception */
		USART_CharReception_Callback();
	}
}

void set_notification_uart(xTaskHandle h)
{
	/* Store the handle of the calling task. */
	xTaskToNotify = h;
}

/**
  * @brief  This function configures USARTx Instance.
  * @note   This function is used to :
  *         -1- Enable GPIO clock and configures the USART pins.
  *         -2- NVIC Configuration for USART interrupts.
  *         -3- Enable the USART peripheral clock and clock source.
  *         -4- Configure USART functional parameters.
  *         -5- Enable USART.
  * @note   Peripheral configuration is minimal configuration from reset values.
  *         Thus, some useless LL unitary functions calls below are provided as
  *         commented examples - setting is default configuration from reset.
  * @param  None
  * @retval None
  */
void Configure_USART(void)
{
	rxrb= CreateRingBuffer(32);

	/* (1) Enable GPIO clock and configures the USART pins *********************/

	/* Enable the peripheral clock of GPIO Port */
	USARTx_GPIO_CLK_ENABLE();

	/* Configure Tx Pin as : Alternate function, High Speed, Push pull, Pull up */
	LL_GPIO_SetPinMode(USARTx_TX_GPIO_PORT, USARTx_TX_PIN, LL_GPIO_MODE_ALTERNATE);
	USARTx_SET_TX_GPIO_AF();
	LL_GPIO_SetPinSpeed(USARTx_TX_GPIO_PORT, USARTx_TX_PIN, LL_GPIO_SPEED_FREQ_HIGH);
	LL_GPIO_SetPinOutputType(USARTx_TX_GPIO_PORT, USARTx_TX_PIN, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinPull(USARTx_TX_GPIO_PORT, USARTx_TX_PIN, LL_GPIO_PULL_UP);

	/* Configure Rx Pin as : Alternate function, High Speed, Push pull, Pull up */
	LL_GPIO_SetPinMode(USARTx_RX_GPIO_PORT, USARTx_RX_PIN, LL_GPIO_MODE_ALTERNATE);
	USARTx_SET_RX_GPIO_AF();
	LL_GPIO_SetPinSpeed(USARTx_RX_GPIO_PORT, USARTx_RX_PIN, LL_GPIO_SPEED_FREQ_HIGH);
	LL_GPIO_SetPinOutputType(USARTx_RX_GPIO_PORT, USARTx_RX_PIN, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinPull(USARTx_RX_GPIO_PORT, USARTx_RX_PIN, LL_GPIO_PULL_UP);

	/* (2) NVIC Configuration for USART interrupts */
	/*  - Set priority for USARTx_IRQn */
	/*  - Enable USARTx_IRQn */
	NVIC_SetPriority(USARTx_IRQn, 0);
	NVIC_EnableIRQ(USARTx_IRQn);

	/* (3) Enable USART peripheral clock and clock source ***********************/
	USARTx_CLK_ENABLE();

	/* Set clock source */
	USARTx_CLK_SOURCE();

	/* (4) Configure USART functional parameters ********************************/

	/* Disable USART prior modifying configuration registers */
	/* Note: Commented as corresponding to Reset value */
	// LL_USART_Disable(USARTx_INSTANCE);

	/* TX/RX direction */
	LL_USART_SetTransferDirection(USARTx_INSTANCE, LL_USART_DIRECTION_TX_RX);

	/* 8 data bit, 1 start bit, 1 stop bit, no parity */
	LL_USART_ConfigCharacter(USARTx_INSTANCE, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1);

	/* No Hardware Flow control */
	/* Reset value is LL_USART_HWCONTROL_NONE */
	// LL_USART_SetHWFlowCtrl(USARTx_INSTANCE, LL_USART_HWCONTROL_NONE);

	/* Oversampling by 16 */
	/* Reset value is LL_USART_OVERSAMPLING_16 */
	//LL_USART_SetOverSampling(USARTx_INSTANCE, LL_USART_OVERSAMPLING_16);

	/* Set Baudrate to 115200 using APB frequency set to 125000000 Hz */
	/* Frequency available for USART peripheral can also be calculated through LL RCC macro */
	/* Ex :

	    In this example, Peripheral Clock is expected to be equal to 125000000 Hz => equal to SystemCoreClock/(AHB_Div * APB_Div)
	*/
	uint32_t Periphclk = HAL_RCC_GetPCLK1Freq(); // PCLK1
	LL_USART_SetBaudRate(USARTx_INSTANCE, Periphclk, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, 115200);

	/* (5) Enable USART *********************************************************/
	LL_USART_Enable(USARTx_INSTANCE);

	/* Polling USART initialisation */
	while((!(LL_USART_IsActiveFlag_TEACK(USARTx_INSTANCE))) || (!(LL_USART_IsActiveFlag_REACK(USARTx_INSTANCE)))) {
	}

	/* Enable RXNE and Error interrupts */
	LL_USART_EnableIT_RXNE(USARTx_INSTANCE);
	LL_USART_DisableIT_TC(USARTx_INSTANCE);
	LL_USART_DisableIT_TXE(USARTx_INSTANCE);
	LL_USART_EnableIT_ERROR(USARTx_INSTANCE);
}

int setup_uart()
{
	Configure_USART();
	return 1;
}

void stop_uart()
{
	HAL_NVIC_DisableIRQ(USARTx_IRQn);
}

size_t read_uart(char * buf, size_t length)
{
	size_t cnt= 0;
	for (int i = 0; i < length; ++i) {
		if(RingBufferEmpty(rxrb)) break;
		uint8_t ch;
		RingBufferGet(rxrb, &ch);
		buf[i]= ch;
		++cnt;
	}

	return cnt;
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
	for (int i = 0; i < length; ++i) {
		LL_USART_TransmitData8(USARTx_INSTANCE, buf[i]);
		while (!LL_USART_IsActiveFlag_TXE(USARTx_INSTANCE)) { }
		LL_USART_ClearFlag_TC(USARTx_INSTANCE);
	}
	return length;
#endif
}
