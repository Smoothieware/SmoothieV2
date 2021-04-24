#include "Builtin.h"



#if defined(STM32H745xx) || defined(STM32H743xx)
#include "stm32h7xx_hal.h"
#include <limits>
#include <stdio.h>

#define ADCx_CHANNEL ADC_CHANNEL_TEMPSENSOR // ADC_CHANNEL_VREFINT | ADC_CHANNEL_VBAT
static ADC_HandleTypeDef    AdcHandle;

static uint32_t TEMP_SENSOR_Init(void)
{
    uint32_t ret = HAL_OK;

    ADC_ChannelConfTypeDef sConfig{0};

    AdcHandle.Instance = ADC3;

    if (HAL_ADC_DeInit(&AdcHandle) != HAL_OK) {
        return HAL_ERROR;
    }

    AdcHandle.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV6;          /* Asynchronous clock mode, input ADC clock divided by 6*/
    AdcHandle.Init.Resolution               = ADC_RESOLUTION_16B;            /* 16-bit resolution for converted data */
    AdcHandle.Init.ScanConvMode             = DISABLE;                       /* Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1) */
    AdcHandle.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;           /* EOC flag picked-up to indicate conversion end */
    AdcHandle.Init.LowPowerAutoWait         = DISABLE;                       /* Auto-delayed conversion feature disabled */
    AdcHandle.Init.ContinuousConvMode       = ENABLE;                        /* Continuous mode enabled (automatic conversion restart after each conversion) */
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

    /* ### - 3 - Channel configuration ######################################## */
    sConfig.Channel                = ADCx_CHANNEL;               /* Sampled channel number */
    sConfig.Rank                   = ADC_REGULAR_RANK_1;         /* Rank of sampled channel number ADCx_CHANNEL */
    sConfig.SamplingTime           = ADC_SAMPLETIME_810CYCLES_5; /* Sampling time (number of clock cycles unit) */
    sConfig.SingleDiff             = ADC_SINGLE_ENDED;           /* Single input channel */
    sConfig.OffsetNumber           = ADC_OFFSET_NONE;            /* No offset subtraction */
    sConfig.Offset                 = 0;                          /* Parameter discarded because offset correction is disabled */
    sConfig.OffsetRightShift       = DISABLE;                    /* No Right Offset Shift */
    sConfig.OffsetSignedSaturation = DISABLE;                    /* No Signed Saturation */
    if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) {
        return HAL_ERROR;
    }

    return ret;
}

static uint32_t TEMP_SENSOR_Start(void)
{
    return HAL_ADC_Start(&AdcHandle);
}

static uint32_t TEMP_SENSOR_Stop(void)
{
    return HAL_ADC_Stop(&AdcHandle);
}

/**
  * @brief  Get JunctionTemp level in 16 bits.
  * @retval JunctionTemp level(0..0xFFFF) / 0xFFFFFFFF : Error
  */
static uint32_t TEMP_SENSOR_GetValue(void)
{
    if (HAL_ADC_PollForConversion(&AdcHandle, 10) == HAL_OK) {
        return HAL_ADC_GetValue(&AdcHandle);
    }else{
        return 0xFFFFFFFF;
    }
}
#define VREFANALOG_VOLTAGE  3300
static float TEMP_SENSOR_read_temp()
{
    uint32_t value= TEMP_SENSOR_GetValue();
    if(value == 0xFFFFFFFF) return std::numeric_limits<float>::infinity();
    return (float)__LL_ADC_CALC_TEMPERATURE(VREFANALOG_VOLTAGE, value, ADC_RESOLUTION_16B);
}

Builtin::Builtin()
{
    if(HAL_OK == TEMP_SENSOR_Init()) {
        valid= true;
    }
}

Builtin::~Builtin()
{
    TEMP_SENSOR_Stop();
    HAL_ADC_DeInit(&AdcHandle);
}

// Set it up
bool Builtin::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    if(!valid) {
        printf("ERROR: Builtin sensor Init failed\n");
        return false;
    }

    if(HAL_OK != TEMP_SENSOR_Start()) {
        printf("ERROR: Builtin sensor Start failed\n");
        return false;
    }
    return true;
}

float Builtin::get_temperature()
{
    float t= TEMP_SENSOR_read_temp();
    return t;
}
#endif
