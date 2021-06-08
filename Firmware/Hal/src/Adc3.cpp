#include "Adc3.h"

#include <cctype>
#include <limits>
#include <stdio.h>
#include <set>

#include "stm32h7xx_hal.h"
#include "Hal_pin.h"

#include "FreeRTOS.h"
#include "message_buffer.h"
#include "task.h"

/*
    ADC3    IN0 Single-ended        ADC3_INP0       PC2_C
    ADC3    IN6 Single-ended        ADC3_INP6       PF10
    ADC3    IN7 Single-ended        ADC3_INP7       PF8
    ADC3    IN8 Single-ended        ADC3_INP8       PF6
    ADC3    IN9 Single-ended        ADC3_INP9       PF4
    ADC3    IN16 Single-ended       ADC3_INP16      PH5

    DEVEBOX
    ADC3    IN10 Single-ended       ADC3_INP10      PC0
*/

Adc3 *Adc3::instance{nullptr};
static ADC_HandleTypeDef AdcHandle;
static std::set<uint32_t> allocated_channels;

static uint32_t adc_channel_lut[] = {
    #ifdef BOARD_DEVEBOX
    ADC_CHANNEL_10,
    #else
    ADC_CHANNEL_0,
    #endif
    ADC_CHANNEL_6,
    ADC_CHANNEL_7,
    ADC_CHANNEL_8,
    ADC_CHANNEL_9,
    ADC_CHANNEL_16,
};

static struct {GPIO_TypeDef* port; uint32_t pin;} adcpinlut[] = {
    #ifdef BOARD_DEVEBOX
    {GPIOC, GPIO_PIN_0},
    #else
    {GPIOC, GPIO_PIN_2},
    #endif
    {GPIOF, GPIO_PIN_10},
    {GPIOF, GPIO_PIN_8},
    {GPIOF, GPIO_PIN_6},
    {GPIOF, GPIO_PIN_4},
    {GPIOH, GPIO_PIN_5},
};

static MessageBufferHandle_t xMessageBuffer;

static uint32_t ADC3_Init(void)
{
    uint32_t ret = HAL_OK;

    AdcHandle.Instance = ADC3;

    if (HAL_ADC_DeInit(&AdcHandle) != HAL_OK) {
        return HAL_ERROR;
    }

    AdcHandle.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV6;          /* Asynchronous clock mode, input ADC clock divided by 6*/
    AdcHandle.Init.Resolution               = ADC_RESOLUTION_16B;            /* 16-bit resolution for converted data */
    AdcHandle.Init.ScanConvMode             = ENABLE;                        /* Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1) */
    AdcHandle.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;           /* EOC flag picked-up to indicate conversion end */
    AdcHandle.Init.LowPowerAutoWait         = DISABLE;                       /* Auto-delayed conversion feature disabled */
    AdcHandle.Init.ContinuousConvMode       = DISABLE;                        /* Continuous mode enabled (automatic conversion restart after each conversion) */
    AdcHandle.Init.NbrOfConversion          = 1;                             /* Parameter discarded because sequencer is disabled */
    AdcHandle.Init.DiscontinuousConvMode    = DISABLE;                       /* Parameter discarded because sequencer is disabled */
    AdcHandle.Init.NbrOfDiscConversion      = 1;                             /* Parameter discarded because sequencer is disabled */
    AdcHandle.Init.ExternalTrigConv         = ADC_SOFTWARE_START;            /* Software start to trig the 1st conversion manually, without external event */
    AdcHandle.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_NONE; /* Parameter discarded because software trigger chosen */
    AdcHandle.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;         /* DR register used as output (DMA mode disabled) */
    AdcHandle.Init.LeftBitShift             = ADC_LEFTBITSHIFT_NONE;         /* Left shift of final results */
    AdcHandle.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;      /* DR register is overwritten with the last conversion result in case of overrun */
    AdcHandle.Init.OversamplingMode         = DISABLE;                       /* Oversampling disable */

    /* Initialize ADC peripheral according to the passed parameters */
    if (HAL_ADC_Init(&AdcHandle) != HAL_OK) {
        return HAL_ERROR;
    }

    /* ### - 2 - Start calibration ############################################ */
    if (HAL_ADCEx_Calibration_Start(&AdcHandle, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        return HAL_ERROR;
    }

    NVIC_SetPriority(ADC3_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(ADC3_IRQn);

    return ret;
}

// MSP init and deinit TODO needs to know which channels are being used
extern "C" void HAL_ADC3_MspInit(ADC_HandleTypeDef *hadc)
{
    __HAL_RCC_ADC3_CLK_ENABLE();
    __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_CLKP);
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
}

extern "C" void HAL_ADC3_MspDeInit(ADC_HandleTypeDef *hadc)
{
    __HAL_RCC_ADC3_FORCE_RESET();
    __HAL_RCC_ADC3_RELEASE_RESET();
    __HAL_RCC_ADC3_CLK_DISABLE();
    NVIC_DisableIRQ(ADC3_IRQn);

    // De-initialize the allocated ADC Channel GPIO pins
    for(auto c : allocated_channels) {
        HAL_GPIO_DeInit(adcpinlut[c].port, adcpinlut[c].pin);
    }
}

extern "C" void ADC3_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&AdcHandle);
}

extern "C" void HAL_ADC3_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    uint32_t value = HAL_ADC_GetValue(hadc);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; /* Initialised to pdFALSE. */
    xMessageBufferSendFromISR(xMessageBuffer, (void *)&value, sizeof(value), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static bool init_channel(int32_t c)
{
    GPIO_InitTypeDef GPIO_InitStruct{0};
    // lookup pin and port
    GPIO_InitStruct.Pin = adcpinlut[c].pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(adcpinlut[c].port, &GPIO_InitStruct);
    allocate_hal_pin(adcpinlut[c].port, adcpinlut[c].pin);
    return true;
}

// select an allocated channel
static bool select_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig{0};
    sConfig.Channel                = channel;                    /* Sampled channel number */
    sConfig.Rank                   = ADC_REGULAR_RANK_1;         /* Rank of sampled channel number ADCx_CHANNEL */
    sConfig.SamplingTime           = ADC_SAMPLETIME_810CYCLES_5; /* Sampling time (number of clock cycles unit) */
    sConfig.SingleDiff             = ADC_SINGLE_ENDED;           /* Single input channel */
    sConfig.OffsetNumber           = ADC_OFFSET_NONE;            /* No offset subtraction */
    sConfig.Offset                 = 0;                          /* Parameter discarded because offset correction is disabled */
    sConfig.OffsetRightShift       = DISABLE;                    /* No Right Offset Shift */
    sConfig.OffsetSignedSaturation = DISABLE;                    /* No Signed Saturation */
    if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) {
        return false;
    }
    return true;
}

// suspend task until we get the result
static uint32_t ADC3_GetValue(void)
{
    const TickType_t xBlockTime = pdMS_TO_TICKS(20);
    uint32_t value;
    size_t xReceivedBytes = xMessageBufferReceive(xMessageBuffer, (void *)&value, sizeof(value), xBlockTime);
    if(xReceivedBytes > 0) {
        return value;
    }

    printf("ERROR: ADC3 - Timeout waiting for conversion\n");
    return 0xFFFFFFFF;
}

#define VREFANALOG_VOLTAGE  3300
static float ADC3_read_temp()
{
    select_channel(ADC_CHANNEL_TEMPSENSOR);
    HAL_ADC_Start_IT(&AdcHandle);
    uint32_t value = ADC3_GetValue();
    HAL_ADC_Stop_IT(&AdcHandle);
    if(value == 0xFFFFFFFF) return std::numeric_limits<float>::infinity();
    return (float)__LL_ADC_CALC_TEMPERATURE(VREFANALOG_VOLTAGE, value, ADC_RESOLUTION_16B);
}

Adc3::Adc3()
{
    if(HAL_OK != ADC3_Init()) {
        printf("ERROR: Adc3 init failed\n");
        return;
    }

    valid = true;
    xMessageBuffer = xMessageBufferCreate(sizeof(uint32_t) + sizeof(size_t));
    if( xMessageBuffer == NULL ) {
        printf("ERROR: Adc3 could not allocate message buffer");
        valid = false;
    }
}

Adc3::~Adc3()
{
    valid = false;
    HAL_ADC_DeInit(&AdcHandle);
    vMessageBufferDelete(xMessageBuffer);
    instance= nullptr;
    allocated_channels.clear();
}

bool Adc3::allocate(int32_t ch) const
{
    if(!valid) return false;
    if(ch >= 0 && ch <= 5) {
        if(allocated_channels.count(ch) == 0) {
            allocated_channels.insert(ch);
            init_channel(ch);
            return true;
        } else {
            printf("ERROR: ADC3 channel %ld is already in use\n", ch);
        }
    }
    return false;
}

float Adc3::read_temp()
{
    if(!valid) return std::numeric_limits<float>::infinity();
    float t = ADC3_read_temp();
    return t;
}

float Adc3::read_voltage(int32_t channel)
{
    // lookup the channel
    if(channel == -1) channel = ADC_CHANNEL_VREFINT;
    else if(channel == -2) channel = ADC_CHANNEL_VBAT;
    else if(channel < 0 || channel > 5) return std::numeric_limits<float>::infinity();
    else if(allocated_channels.count(channel) == 0) return std::numeric_limits<float>::infinity();
    else channel = adc_channel_lut[channel];

    select_channel(channel);
    HAL_ADC_Start_IT(&AdcHandle);
    uint32_t value = ADC3_GetValue();
    HAL_ADC_Stop_IT(&AdcHandle);
    if(value == 0xFFFFFFFF) return std::numeric_limits<float>::infinity();
    float v = 3.3F * ((float)value / get_max_value());
    return v;
}
