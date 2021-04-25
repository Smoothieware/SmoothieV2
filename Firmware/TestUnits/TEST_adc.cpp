#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "Adc.h"
#include "SlowTicker.h"

#include "FreeRTOS.h"
#include "task.h"

#include "benchmark_timer.h"

using systime_t = uint32_t;

REGISTER_TEST(ADCTest, adc_names)
{
    // can't use 0,1,4 on nucleo
    Adc *adc = new Adc("ADC1_2");
    TEST_ASSERT_TRUE(adc->is_valid());
    TEST_ASSERT_EQUAL_INT(1, adc->get_channel());

    // check it won't let us create a duplicate
    Adc *dummy = new Adc("ADC1_2");
    TEST_ASSERT_FALSE(dummy->is_valid());
    delete dummy;

    // illegal name
    dummy = new Adc("ADC2_1");
    TEST_ASSERT_FALSE(dummy->is_valid());
    delete dummy;

    // illegal channel
    dummy = new Adc("ADC1_16");
    TEST_ASSERT_FALSE(dummy->is_valid());
    delete dummy;


    delete adc;
    // should be able to create it now
    dummy = new Adc("ADC1_2");
    TEST_ASSERT_TRUE(dummy->is_valid());
    delete dummy;
}

REGISTER_TEST(ADCTest, two_adc_channels)
{
    // Use ADC2 and ADC3 on PF11 and PF12
    Adc *adc1 = new Adc("ADC1_2");
    Adc *adc2 = new Adc("ADC1_3");

    TEST_ASSERT_TRUE(Adc::post_config_setup());

    TEST_ASSERT_TRUE(adc1->is_valid());
    TEST_ASSERT_EQUAL_INT(0, adc1->get_channel());

    TEST_ASSERT_TRUE(adc2->is_valid());
    TEST_ASSERT_EQUAL_INT(2, adc2->get_channel());

    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %lu\n", max_adc_value);

    // give it time to accumulate the 32 samples
    vTaskDelay(pdMS_TO_TICKS(32 * 10 * 2 + 100));

    for (int i = 0; i < 10; ++i) {
        uint16_t v = adc2->read();
        float volts = 3.3F * (v / (float)max_adc_value);
        printf("adc2= %04X, volts= %10.4f\n", v, volts);

        v = adc1->read();
        volts = 3.3F * (v / (float)max_adc_value);
        printf("adc1= %04X, volts= %10.4f\n", v, volts);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    TEST_ASSERT_TRUE(adc1->get_errors() == 0);
    TEST_ASSERT_TRUE(adc2->get_errors() == 0);

    TEST_ASSERT_TRUE(Adc::stop());

    Adc::deinit();

    delete adc1;
    delete adc2;
}

#include "stm32h7xx_hal.h"

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
  * @brief  Get JunctionTemp level in 12 bits.
  * @retval JunctionTemp level(0..0xFFF) / 0xFFFFFFFF : Error
  */
static uint32_t TEMP_SENSOR_GetValue(void)
{
    if (HAL_ADC_PollForConversion(&AdcHandle, 10) == HAL_OK) {
        return HAL_ADC_GetValue(&AdcHandle);
    }else{
        printf("HAL_ADC_PollForConversion failed\n");
        return 0;
    }
}

#ifdef USE_FULL_LL_DRIVER
#include "stm32h7xx_ll_rcc.h"
#endif

#include "benchmark_timer.h"
#define VREFANALOG_VOLTAGE  3300
#define Calibration_ON

REGISTER_TEST(ADCTest, read_onboard_temp_sensor)
{
#ifdef USE_FULL_LL_DRIVER
    uint32_t hz = LL_RCC_GetADCClockFreq(LL_RCC_ADC_CLKSOURCE_CLKP);
    printf("DEBUG: ADC clk source: %lu hz\n", hz);
    // divide clock by 6 from init setting and another 2 which is fixed.
    hz /= 2;
    hz /= 6;
    printf("should take %f us per sample\n", (1e6f * (810.5F + 8.5F))/hz);
#endif
    uint32_t values[10], elt[10];
    TEST_ASSERT_EQUAL_INT(HAL_OK, TEMP_SENSOR_Init());
    TEST_ASSERT_EQUAL_INT(HAL_OK, TEMP_SENSOR_Start());
    for (int i = 0; i < 10; ++i) {
        uint32_t s= benchmark_timer_start();
        values[i] = TEMP_SENSOR_GetValue();
        elt[i]= benchmark_timer_elapsed(s);
    }

    TEST_ASSERT_EQUAL_INT(HAL_OK, TEMP_SENSOR_Stop());
    HAL_ADC_DeInit(&AdcHandle);

    for (int i = 0; i < 10; ++i) {
        uint32_t JTemp;
        if(ADCx_CHANNEL == ADC_CHANNEL_TEMPSENSOR) {
            #ifdef Calibration_ON
            JTemp = __LL_ADC_CALC_TEMPERATURE(VREFANALOG_VOLTAGE, values[i], ADC_RESOLUTION_16B);
            #else
            JTemp = ((80 * values[i]) / 4095) + 30;
            #endif

        }else if(ADCx_CHANNEL == ADC_CHANNEL_VREFINT) {
            JTemp = (values[i] * 3300) / 0xFFFF;
        }
        printf("adc: %lu, temp: %lu, took: %lu us\n", values[i], JTemp, benchmark_timer_as_us(elt[i]));
    }
}
