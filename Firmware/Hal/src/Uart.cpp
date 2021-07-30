#include "Uart.h"
//#include "Hal_pin.h"

#include "stm32h7xx_hal.h"
#ifdef USE_FULL_LL_DRIVER
#include "stm32h7xx_ll_rcc.h"
#endif

#include <stdio.h>
#include <cstring>

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "Hal_pin.h"

UART *UART::uart_channel[2];
// static
UART *UART::getInstance(int channel)
{
    if(channel >= 2) return nullptr;
    if(uart_channel[channel] == nullptr) {
        uart_channel[channel] = new UART(channel);
    }

    return uart_channel[channel];
}

void UART::deleteInstance(int channel)
{
    if(channel >= 2) return;
    if(uart_channel[channel] == nullptr) {
        return;
    }
    if(uart_channel[channel]->valid()) {
        HAL_UART_AbortReceive((UART_HandleTypeDef*)uart_channel[channel]->_huart);
    }
    delete uart_channel[channel];
    uart_channel[channel] = nullptr;
}

UART::UART(int channel)
{
    _valid = false;
    _channel = channel;
}

UART::~UART()
{
    if(_valid) {
        HAL_UART_DeInit((UART_HandleTypeDef*)_huart);
        free(_huart);
        if(rx_rb != nullptr) vStreamBufferDelete(rx_rb);
        if(tx_rb != nullptr) vStreamBufferDelete(tx_rb);
    }
}

// Hal stuff

/* first spare Uart
| UART3_RX   | PB11    |
| UART3_TX   | PD8     |
*/
#define USART1x                           USART3
#define USART1x_CLK_ENABLE()              __HAL_RCC_USART3_CLK_ENABLE()
#define USART1x_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
#define USART1x_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOD_CLK_ENABLE()
#define DMAx_CLK_ENABLE()                 __HAL_RCC_DMA1_CLK_ENABLE()
#define RCC_PERIPHCLK_USART1x             RCC_PERIPHCLK_USART3
#define RCC_USART1xCLKSOURCE              RCC_USART234578CLKSOURCE_PCLK1

#define USART1x_FORCE_RESET()             __HAL_RCC_USART3_FORCE_RESET()
#define USART1x_RELEASE_RESET()           __HAL_RCC_USART3_RELEASE_RESET()

/* Definition for USART1x Pins */
#define USART1x_TX_PIN                    GPIO_PIN_8
#define USART1x_TX_GPIO_PORT              GPIOD
#define USART1x_TX_AF                     GPIO_AF7_USART3
#define USART1x_RX_PIN                    GPIO_PIN_11
#define USART1x_RX_GPIO_PORT              GPIOB
#define USART1x_RX_AF                     GPIO_AF7_USART3

/* Definition for USART1x's DMA */
#define USART1x_TX_DMA_STREAM             DMA1_Stream7
#define USART1x_RX_DMA_STREAM             DMA1_Stream5

#define USART1x_TX_DMA_CHANNEL            DMA_REQUEST_USART3_TX
#define USART1x_RX_DMA_CHANNEL            DMA_REQUEST_USART3_RX

/* Definition for USART1x's NVIC */
#define USART1x_DMA_TX_IRQn               DMA1_Stream7_IRQn
#define USART1x_DMA_RX_IRQn               DMA1_Stream5_IRQn
#define USART1x_DMA_TX_IRQHandler         DMA1_Stream7_IRQHandler
#define USART1x_DMA_RX_IRQHandler         DMA1_Stream5_IRQHandler

/* Definition for USART1x's NVIC */
#define USART1x_IRQn                      USART3_IRQn
#ifndef BOARD_NUCLEO
#define USART1x_IRQHandler                USART3_IRQHandler
#endif

/* second spare uart on PRIME
| UART4_RX   | PH14    | PB8 on Nucleo |
| UART4_TX   | PB9     |               |
*/
#define USART2x                           UART4
#define USART2x_CLK_ENABLE()              __HAL_RCC_UART4_CLK_ENABLE()
#define USART2x_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOH_CLK_ENABLE()
#define USART2x_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
#define RCC_PERIPHCLK_USART2x             RCC_PERIPHCLK_UART4
#define RCC_USART2xCLKSOURCE              RCC_USART234578CLKSOURCE_PCLK1

#define USART2x_FORCE_RESET()             __HAL_RCC_UART4_FORCE_RESET()
#define USART2x_RELEASE_RESET()           __HAL_RCC_UART4_RELEASE_RESET()

/* Definition for USART2x Pins */
#define USART2x_TX_PIN                    GPIO_PIN_9
#define USART2x_TX_GPIO_PORT              GPIOB
#define USART2x_TX_AF                     GPIO_AF8_UART4
#ifdef BOARD_NUCLEO
#define USART2x_RX_PIN                    GPIO_PIN_8
#define USART2x_RX_GPIO_PORT              GPIOB
#define USART2x_RX_AF                     GPIO_AF8_UART4
#else
#define USART2x_RX_PIN                    GPIO_PIN_14
#define USART2x_RX_GPIO_PORT              GPIOH
#define USART2x_RX_AF                     GPIO_AF8_UART4
#endif
/* Definition for USART2x's DMA */
#define USART2x_TX_DMA_STREAM             DMA1_Stream6
#define USART2x_RX_DMA_STREAM             DMA1_Stream4

#define USART2x_TX_DMA_CHANNEL            DMA_REQUEST_UART4_TX
#define USART2x_RX_DMA_CHANNEL            DMA_REQUEST_UART4_RX

/* Definition for USART2x's NVIC */
#define USART2x_DMA_TX_IRQn               DMA1_Stream6_IRQn
#define USART2x_DMA_RX_IRQn               DMA1_Stream4_IRQn
#define USART2x_DMA_TX_IRQHandler         DMA1_Stream6_IRQHandler
#define USART2x_DMA_RX_IRQHandler         DMA1_Stream4_IRQHandler

/* Definition for USART2x's NVIC */
#define USART2x_IRQn                      UART4_IRQn
#ifndef BOARD_DEVEBOX
#define USART2x_IRQHandler                UART4_IRQHandler
#endif

#define RX_BUFFER_SIZE   (32*4) // 128
// cache aligned
static uint8_t RXBuffer[2][RX_BUFFER_SIZE] __attribute__((section (".sram_1_bss"), aligned(32))); // put in SRAM_1

bool UART::init(settings_t set)
{
    if(_valid) {
        if(memcmp(&settings, &set, sizeof(settings_t)) != 0) {
            printf("ERROR: UART channel %d, already set with different parameters\n", _channel);
            return false;
        }
        return true;
    }
    settings= set;

    UART_HandleTypeDef UartHandle = {0};
    UartHandle.Instance        = _channel == 0 ? USART1x : USART2x;
    UartHandle.Init.BaudRate   = settings.baudrate;
    UartHandle.Init.WordLength = settings.bits == 8 ? UART_WORDLENGTH_8B : settings.bits == 7 ? UART_WORDLENGTH_7B : UART_WORDLENGTH_9B;
    UartHandle.Init.StopBits   = settings.stop_bits == 1 ? UART_STOPBITS_1 : settings.stop_bits == 2 ? UART_STOPBITS_2 : UART_STOPBITS_0_5;
    UartHandle.Init.Parity     = settings.parity == 0 ? UART_PARITY_NONE : settings.parity == 1 ? UART_PARITY_ODD : UART_PARITY_EVEN;
    UartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    UartHandle.Init.Mode       = UART_MODE_TX_RX;
    UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;

    if(HAL_UART_Init(&UartHandle) != HAL_OK) {
        /* Initialization Error */
        return false;
    }

    _huart = malloc(sizeof(UART_HandleTypeDef));
    memcpy(_huart, &UartHandle, sizeof(UART_HandleTypeDef));
    _valid = true;

    // create ring buffers
    rx_rb = xStreamBufferCreate(1024, 1);;
    tx_rb = xStreamBufferCreate(1024, 1);;

    /* Initializes Rx sequence using Reception To Idle event API.
      As DMA channel associated to UART Rx is configured as Circular,
      reception is endless.
      If reception has to be stopped, call to HAL_UART_AbortReceive() could be used.

      Use of HAL_UARTEx_ReceiveToIdle_DMA service, will generate calls to
      user defined HAL_UARTEx_RxEventCallback callback for each occurrence of
      following events :
      - DMA RX Half Transfer event (HT)
      - DMA RX Transfer Complete event (TC)
      - IDLE event on UART Rx line (indicating a pause is UART reception flow)
    */
    if(HAL_OK != HAL_UARTEx_ReceiveToIdle_DMA((UART_HandleTypeDef*)_huart, RXBuffer[_channel], RX_BUFFER_SIZE)) {
        // failed
        printf("ERROR: UART HAL_UARTEx_ReceiveToIdle_DMA failed\n");
        free(_huart);
        vStreamBufferDelete(rx_rb);
        vStreamBufferDelete(tx_rb);
        _huart= nullptr;
        rx_rb= nullptr;
        tx_rb= nullptr;
        return false;
    }

    return true;
}

static DMA_HandleTypeDef hdma_tx[2];
static DMA_HandleTypeDef hdma_rx[2];
extern "C" void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef  GPIO_InitStruct{0};
    RCC_PeriphCLKInitTypeDef RCC_PeriphClkInit{0};

    if(huart->Instance == USART1x) {
        USART1x_TX_GPIO_CLK_ENABLE();
        USART1x_RX_GPIO_CLK_ENABLE();
        RCC_PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1x;
        RCC_PeriphClkInit.Usart234578ClockSelection = RCC_USART1xCLKSOURCE;
        HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphClkInit);
        USART1x_CLK_ENABLE();
        DMAx_CLK_ENABLE();
        GPIO_InitStruct.Pin       = USART1x_TX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = USART1x_TX_AF;
        HAL_GPIO_Init(USART1x_TX_GPIO_PORT, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = USART1x_RX_PIN;
        GPIO_InitStruct.Alternate = USART1x_RX_AF;
        HAL_GPIO_Init(USART1x_RX_GPIO_PORT, &GPIO_InitStruct);
        allocate_hal_pin(USART1x_TX_GPIO_PORT, USART1x_TX_PIN);
        allocate_hal_pin(USART1x_RX_GPIO_PORT, USART1x_RX_PIN);

        hdma_tx[0].Instance                 = USART1x_TX_DMA_STREAM;
        hdma_tx[0].Init.Request             = USART1x_TX_DMA_CHANNEL;
        hdma_tx[0].Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_tx[0].Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_tx[0].Init.MemInc              = DMA_MINC_ENABLE;
        hdma_tx[0].Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_tx[0].Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_tx[0].Init.Mode                = DMA_NORMAL;
        hdma_tx[0].Init.Priority            = DMA_PRIORITY_LOW;
        hdma_tx[0].Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        hdma_tx[0].Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_tx[0].Init.MemBurst            = DMA_MBURST_INC4;
        hdma_tx[0].Init.PeriphBurst         = DMA_PBURST_INC4;
        HAL_DMA_Init(&hdma_tx[0]);
        __HAL_LINKDMA(huart, hdmatx, hdma_tx[0]);

        hdma_rx[0].Instance                 = USART1x_RX_DMA_STREAM;
        hdma_rx[0].Init.Request             = USART1x_RX_DMA_CHANNEL;
        hdma_rx[0].Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_rx[0].Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_rx[0].Init.MemInc              = DMA_MINC_ENABLE;
        hdma_rx[0].Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_rx[0].Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_rx[0].Init.Mode                = DMA_CIRCULAR;
        hdma_rx[0].Init.Priority            = DMA_PRIORITY_HIGH;
        hdma_rx[0].Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        hdma_rx[0].Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_rx[0].Init.MemBurst            = DMA_MBURST_INC4;
        hdma_rx[0].Init.PeriphBurst         = DMA_PBURST_INC4;
        HAL_DMA_Init(&hdma_rx[0]);
        __HAL_LINKDMA(huart, hdmarx, hdma_rx[0]);

        NVIC_SetPriority(USART1x_DMA_TX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY+1);
        NVIC_EnableIRQ(USART1x_DMA_TX_IRQn);
        NVIC_SetPriority(USART1x_DMA_RX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(USART1x_DMA_RX_IRQn);
        NVIC_SetPriority(USART1x_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY+1);
        NVIC_EnableIRQ(USART1x_IRQn);

    } else if(huart->Instance == USART2x) {
        USART2x_TX_GPIO_CLK_ENABLE();
        USART2x_RX_GPIO_CLK_ENABLE();
        RCC_PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2x;
        RCC_PeriphClkInit.Usart234578ClockSelection = RCC_USART2xCLKSOURCE;
        HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphClkInit);
        USART2x_CLK_ENABLE();
        DMAx_CLK_ENABLE();
        GPIO_InitStruct.Pin       = USART2x_TX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = USART2x_TX_AF;
        HAL_GPIO_Init(USART2x_TX_GPIO_PORT, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = USART2x_RX_PIN;
        GPIO_InitStruct.Alternate = USART2x_RX_AF;
        HAL_GPIO_Init(USART2x_RX_GPIO_PORT, &GPIO_InitStruct);
        allocate_hal_pin(USART2x_TX_GPIO_PORT, USART2x_TX_PIN);
        allocate_hal_pin(USART2x_RX_GPIO_PORT, USART2x_RX_PIN);

        hdma_tx[1].Instance                 = USART2x_TX_DMA_STREAM;
        hdma_tx[1].Init.Request             = USART2x_TX_DMA_CHANNEL;
        hdma_tx[1].Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_tx[1].Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_tx[1].Init.MemInc              = DMA_MINC_ENABLE;
        hdma_tx[1].Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_tx[1].Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_tx[1].Init.Mode                = DMA_NORMAL;
        hdma_tx[1].Init.Priority            = DMA_PRIORITY_LOW;
        hdma_tx[1].Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        hdma_tx[1].Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_tx[1].Init.MemBurst            = DMA_MBURST_INC4;
        hdma_tx[1].Init.PeriphBurst         = DMA_PBURST_INC4;
        HAL_DMA_Init(&hdma_tx[1]);
        __HAL_LINKDMA(huart, hdmatx, hdma_tx[1]);

        hdma_rx[1].Instance                 = USART2x_RX_DMA_STREAM;
        hdma_rx[1].Init.Request             = USART2x_RX_DMA_CHANNEL;
        hdma_rx[1].Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_rx[1].Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_rx[1].Init.MemInc              = DMA_MINC_ENABLE;
        hdma_rx[1].Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_rx[1].Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_rx[1].Init.Mode                = DMA_CIRCULAR;
        hdma_rx[1].Init.Priority            = DMA_PRIORITY_HIGH;
        hdma_rx[1].Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        hdma_rx[1].Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_rx[1].Init.MemBurst            = DMA_MBURST_INC4;
        hdma_rx[1].Init.PeriphBurst         = DMA_PBURST_INC4;
        HAL_DMA_Init(&hdma_rx[1]);
        __HAL_LINKDMA(huart, hdmarx, hdma_rx[1]);

        NVIC_SetPriority(USART2x_DMA_TX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY+1);
        NVIC_EnableIRQ(USART2x_DMA_TX_IRQn);
        NVIC_SetPriority(USART2x_DMA_RX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
        NVIC_EnableIRQ(USART2x_DMA_RX_IRQn);
        NVIC_SetPriority(USART2x_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY+1);
        NVIC_EnableIRQ(USART2x_IRQn);
    }
}

/**
  * @brief UART MSP De-Initialization
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO, DMA and NVIC configuration to their default state
  * @param huart: UART handle pointer
  * @retval None
  */
extern "C" void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1x) {
        USART1x_FORCE_RESET();
        USART1x_RELEASE_RESET();
        HAL_GPIO_DeInit(USART1x_TX_GPIO_PORT, USART1x_TX_PIN);
        HAL_GPIO_DeInit(USART1x_RX_GPIO_PORT, USART1x_RX_PIN);
        HAL_DMA_DeInit(huart->hdmatx);
        HAL_DMA_DeInit(huart->hdmarx);
        HAL_NVIC_DisableIRQ(USART1x_DMA_TX_IRQn);
        HAL_NVIC_DisableIRQ(USART1x_DMA_RX_IRQn);
        NVIC_DisableIRQ(USART1x_IRQn);

    } else if(huart->Instance == USART2x) {
        USART2x_FORCE_RESET();
        USART2x_RELEASE_RESET();
        HAL_GPIO_DeInit(USART2x_TX_GPIO_PORT, USART2x_TX_PIN);
        HAL_GPIO_DeInit(USART2x_RX_GPIO_PORT, USART2x_RX_PIN);
        HAL_DMA_DeInit(huart->hdmatx);
        HAL_DMA_DeInit(huart->hdmarx);
        NVIC_DisableIRQ(USART2x_DMA_TX_IRQn);
        NVIC_DisableIRQ(USART2x_DMA_RX_IRQn);
        NVIC_DisableIRQ(USART2x_IRQn);
    }
}

extern "C" void USART1x_DMA_RX_IRQHandler(void)
{
    HAL_DMA_IRQHandler((DMA_HandleTypeDef*)((UART_HandleTypeDef*)UART::uart_channel[0]->get_huart())->hdmarx);
}
extern "C" void USART1x_DMA_TX_IRQHandler(void)
{
    HAL_DMA_IRQHandler((DMA_HandleTypeDef*)((UART_HandleTypeDef*)UART::uart_channel[0]->get_huart())->hdmatx);
}
extern "C" void USART1x_IRQHandler(void)
{
    HAL_UART_IRQHandler((UART_HandleTypeDef*)UART::uart_channel[0]->get_huart());
}
extern "C" void USART2x_DMA_RX_IRQHandler(void)
{
    HAL_DMA_IRQHandler((DMA_HandleTypeDef*)((UART_HandleTypeDef*)UART::uart_channel[1]->get_huart())->hdmarx);
}
extern "C" void USART2x_DMA_TX_IRQHandler(void)
{
    HAL_DMA_IRQHandler((DMA_HandleTypeDef*)((UART_HandleTypeDef*)UART::uart_channel[1]->get_huart())->hdmatx);
}
extern "C" void USART2x_IRQHandler(void)
{
    HAL_UART_IRQHandler((UART_HandleTypeDef*)UART::uart_channel[1]->get_huart());
}

// we need a cache aligned buffer to transmit from preferably in SRAM_1 .sram_1_bss
#define TX_BUFFER_SIZE (32*4) // 128
static uint8_t TXBuffer[2][TX_BUFFER_SIZE] __attribute__((section (".sram_1_bss"), aligned(32)));

/**
  * @brief  Tx Transfer completed callback
  * @param  huart: UART handle.
  * @retval None
  */
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1x) {
        UART::uart_channel[0]->tx_cplt_callback();

    } else if(huart->Instance == USART2x) {
        UART::uart_channel[1]->tx_cplt_callback();
    }
}

// ISR
void UART::tx_cplt_callback()
{
    // start again if there is data to be sent
    // transfer as much as we can to DMA buffer, do cache management and start DMA
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.
    size_t cnt = xStreamBufferReceiveFromISR(tx_rb, TXBuffer[_channel], TX_BUFFER_SIZE, &xHigherPriorityTaskWoken);
    tx_dma_current_len= cnt;
    //printf("Sending %d\n", cnt);
    if(cnt > 0) {
        // make sure cache is flushed to RAM so the DMA can read the correct data
        // NOTE this relies on the buffer being 32 byte aligned and a multiple of 32 bytes in size
        SCB_CleanDCache_by_Addr((uint32_t*)TXBuffer[_channel], TX_BUFFER_SIZE);

        // start write
        if(HAL_UART_Transmit_DMA((UART_HandleTypeDef*)_huart, TXBuffer[_channel], cnt) != HAL_OK) {
            /* Transfer error in transmission process */
            printf("ERROR: UART HAL_UART_Transmit_DMA failed\n");
        }
        // HACK ALERT, there is a bug in the HAL that causes the normal operation to be unreliable
        // and higher priority interrupts can delay this in UART_DMATransmitCplt() where it no longer works.
        // this hack seems to work, and fix the issue. symptoms are that HAL_UART_TxCpltCallback does not get called.
        SET_BIT(((UART_HandleTypeDef*)_huart)->Instance->CR1, USART_CR1_TCIE);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

size_t UART::write(uint8_t *buf, uint16_t len, uint32_t timeout)
{
    size_t cnt = 0;
    // this needs to be protected from tx ISR concurrent access, so just disable the TX interrupt
    //HAL_NVIC_DisableIRQ(_channel == 0 ? USART1x_DMA_TX_IRQn : USART2x_DMA_TX_IRQn);
    HAL_NVIC_DisableIRQ(_channel == 0 ? USART1x_IRQn : USART2x_IRQn);
    uint16_t cur_len= tx_dma_current_len;
    HAL_NVIC_EnableIRQ(_channel == 0 ? USART1x_IRQn : USART2x_IRQn);
    //HAL_NVIC_EnableIRQ(_channel == 0 ? USART1x_DMA_TX_IRQn : USART2x_DMA_TX_IRQn);

    if(len > 1024) len= 1024; // limit to max size of streambuffer

    if(cur_len == 0) {
        if(!xStreamBufferIsEmpty(tx_rb)) { printf("WARNING: Stream buffer is not empty\n"); }

        // TX DMA is not running so we cannot get interrupted by it
        // copy as much of buffer as we can to TXBuffer, and rest to txStreamBuffer
        uint16_t n = len > TX_BUFFER_SIZE ? TX_BUFFER_SIZE : len;
        memcpy(TXBuffer[_channel], buf, n);
        cnt = n;
        if(n < len) {
            // copy the rest to the stream buffer and block for timeout
            cnt += xStreamBufferSend(tx_rb, &buf[n], len - n, timeout);
        }

        UART_HandleTypeDef *huart = (UART_HandleTypeDef*)_huart;
        // clean cache so DMA can see what we wrote
        SCB_CleanDCache_by_Addr((uint32_t*)TXBuffer[_channel], TX_BUFFER_SIZE);
        tx_dma_current_len= n;
        // then start DMA
        if(HAL_UART_Transmit_DMA(huart, TXBuffer[_channel], n) != HAL_OK) {
            // failed
            printf("ERROR: UART HAL_UART_Transmit_DMA failed\n");
            cnt = 0;
        }
        // HACK ALERT, there is a bug in the HAL that causes the normal operation to be unreliable
        // and higher priority interrupts can delay this in UART_DMATransmitCplt() where it no longer works.
        // this hack seems to work, and fix the issue. symptoms are that HAL_UART_TxCpltCallback does not get called.
        SET_BIT(huart->Instance->CR1, USART_CR1_TCIE);

        //printf("Wrote direct\n");

    } else {
        // copy as much of buffer as we can to txStreamBuffer
        // it will be picked up when current tx completes
        cnt = xStreamBufferSend(tx_rb, buf, len, timeout);
        //printf("Wrote to streambuffer\n");
    }

    return cnt;
}

/**
  * @brief  User implementation of the Reception Event Callback
  *         (Rx event notification called after use of advanced reception service).
  * @param  huart UART handle
  * @param  Size  Number of data available in application reception buffer (indicates a position in
  *               reception buffer until which, data are available)
  * @retval None
  */
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    if(huart->Instance == USART1x) {
        UART::uart_channel[0]->rx_event_callback(size);

    } else if(huart->Instance == USART2x) {
        UART::uart_channel[1]->rx_event_callback(size);
    }
}

// ISR
void UART::rx_event_callback(uint16_t size)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.
    /* Check if number of received data in reception buffer has changed */
    if (size != old_pos) {
        // invalidate the cache so we can read the data the DMA put into it
        // NOTE this relies on the buffer being 32 byte aligned and a multiple of 32 bytes in size
        SCB_InvalidateDCache_by_Addr((uint32_t*)RXBuffer[_channel], RX_BUFFER_SIZE);
        /* Check if position of index in reception buffer has simply be increased
           or if end of buffer has been reached */
        if (size > old_pos) {
            /* Current position is higher than previous one */
            uint16_t uwNbReceivedChars = size - old_pos;
            /* Copy received data into ring buffer for evacuation */
            size_t n = xStreamBufferSendFromISR(rx_rb, &RXBuffer[_channel][old_pos], uwNbReceivedChars, &xHigherPriorityTaskWoken);
            if(n != uwNbReceivedChars) { overflow += uwNbReceivedChars; }

        } else {
            /* Current position is lower than previous one : end of buffer has been reached */
            /* First copy data from current position till end of buffer */
            uint16_t uwNbReceivedChars = RX_BUFFER_SIZE - old_pos;
            /* Copy received data into ring buffer for evacuation */
            size_t n = xStreamBufferSendFromISR(rx_rb, &RXBuffer[_channel][old_pos], uwNbReceivedChars, &xHigherPriorityTaskWoken);
            if(n != uwNbReceivedChars) { overflow += uwNbReceivedChars; }

            /* Check and continue with beginning of buffer */
            if (size > 0) {
                n = xStreamBufferSendFromISR(rx_rb, RXBuffer[_channel], size, &xHigherPriorityTaskWoken);
                if(n != size) { overflow += size; }
            }
        }
    }
    /* Update old_pos as new reference of position in User Rx buffer that
       indicates position to which data have been processed */
    old_pos = size;
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

size_t UART::read(uint8_t *buf, uint16_t len, uint32_t timeout)
{
    // read upto len bytes or timeout if none
    size_t cnt= xStreamBufferReceive(rx_rb, buf, len, timeout);
    return cnt;
}
