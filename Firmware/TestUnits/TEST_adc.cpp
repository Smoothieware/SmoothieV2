#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "Adc.h"
#include "Adc3.h"
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
    TEST_ASSERT_EQUAL_INT(2, adc->get_channel());

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

extern uint32_t adc_get_time();
REGISTER_TEST(ADCTest, two_adc_channels)
{
    // Use ADC0 and ADC3 on PA0 and PB0
    Adc *adc1 = new Adc("ADC1_0");
    Adc *adc2 = new Adc("ADC1_3");

    TEST_ASSERT_TRUE(Adc::post_config_setup());

    TEST_ASSERT_TRUE(adc1->is_valid());
    TEST_ASSERT_EQUAL_INT(0, adc1->get_channel());

    TEST_ASSERT_TRUE(adc2->is_valid());
    TEST_ASSERT_EQUAL_INT(3, adc2->get_channel());

    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %lu\n", max_adc_value);

    // give it time to accumulate the 32 samples
    vTaskDelay(pdMS_TO_TICKS(32 * 10 * 2 + 1000));

    uint16_t min1=0xFFFF, max1= 0;
    uint16_t min2=0xFFFF, max2= 0;
    for (int i = 0; i < 10; ++i) {
        uint16_t v2 = adc2->read();
        float volts = 3.3F * (v2 / (float)max_adc_value);
        printf("adc2= %04X, volts= %10.4f\n", v2, volts);
        if(v2 < min2) min2= v2;
        if(v2 > max2) max2= v2;

        uint16_t v1 = adc1->read();
        volts = 3.3F * (v1 / (float)max_adc_value);
        printf("adc1= %04X, volts= %10.4f\n", v1, volts);
        if(v1 < min1) min1= v1;
        if(v1 > max1) max1= v1;
        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz
        // takes 2461us between samples for 2 channels
        //printf("time= %lu us\n", adc_get_time());
    }
    printf("Min1= %d, Max1= %d, spread1= %d (%f %%)\n", min1, max1, max1-min1, (max1-min1)*100.0F/(float)max_adc_value);
    printf("Min2= %d, Max2= %d, spread2= %d (%f %%)\n", min2, max2, max2-min2, (max2-min2)*100.0F/(float)max_adc_value);

    TEST_ASSERT_TRUE(adc1->get_errors() == 0);
    TEST_ASSERT_TRUE(adc2->get_errors() == 0);

    TEST_ASSERT_TRUE(Adc::stop());

    Adc::deinit();

    delete adc1;
    delete adc2;
}

#if 0
extern "C" void xNetworkDisablePHY();
REGISTER_TEST(ADCTest, test_adc_noise)
{
    // Use ADC0
    Adc *adc1 = new Adc("ADC1_0");

    TEST_ASSERT_TRUE(Adc::post_config_setup());

    TEST_ASSERT_TRUE(adc1->is_valid());
    TEST_ASSERT_EQUAL_INT(0, adc1->get_channel());

    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %lu\n", max_adc_value);

    for (int j = 0; j < 2; ++j) {
        if(j==1) {
            xNetworkDisablePHY();
            printf("Disable PHY....\n");
        }else{
            printf("PHY is Enabled....\n");
        }

        // give it time to accumulate the 32 samples
        vTaskDelay(pdMS_TO_TICKS(32 * 10 * 2 + 100));

        uint16_t v;
        float volts;
        uint16_t min=0xFFFF, max= 0;
        for (int i = 0; i < 10; ++i) {
            v = adc1->read();
            volts = 3.3F * (v / (float)max_adc_value);
            printf("adc1= %04X, volts= %10.4f\n", v, volts);
            if(v < min) min= v;
            if(v > max) max= v;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        printf("Min= %d, Max= %d, spread= %d (%f %%)\n", min, max, max-min, (max-min)*100.0F/(float)max_adc_value);
    }

    TEST_ASSERT_TRUE(adc1->get_errors() == 0);

    TEST_ASSERT_TRUE(Adc::stop());

    Adc::deinit();

    delete adc1;
}
#endif

#ifdef USE_FULL_LL_DRIVER
#include "stm32h7xx_ll_rcc.h"
#endif

#include "benchmark_timer.h"

REGISTER_TEST(ADCTest, adc3_read_temp)
{
#ifdef USE_FULL_LL_DRIVER
    uint32_t hz = LL_RCC_GetADCClockFreq(LL_RCC_ADC_CLKSOURCE_CLKP);
    printf("DEBUG: ADC clk source: %lu hz\n", hz);
    // divide clock by 6 from init setting and another 2 which is fixed.
    hz /= 2;
    hz /= 6;
    printf("should take %f us per sample\n", (1e6f * (810.5F + 8.5F))/hz);
#endif
    Adc3 *adc= Adc3::getInstance();
    TEST_ASSERT_TRUE(adc->is_valid());
    uint32_t values[10], elt[10];
    for (int i = 0; i < 10; ++i) {
        uint32_t s= benchmark_timer_start();
        values[i] = adc->read_temp();
        elt[i]= benchmark_timer_elapsed(s);
    }

    for (int i = 0; i < 10; ++i) {
        printf("temp: %lu, took: %lu us\n", values[i], benchmark_timer_as_us(elt[i]));
    }
    Adc3::deinit();
}

REGISTER_TEST(ADCTest, adc3_read_voltage)
{
    Adc3 *adc= Adc3::getInstance();
    TEST_ASSERT_TRUE(adc->is_valid());

    float a= adc->read_voltage(0);
    printf("voltage= %f\n", a);
    TEST_ASSERT_FLOAT_IS_INF(a);

    TEST_ASSERT_TRUE(adc->allocate(0));
    a= adc->read_voltage(0);
    TEST_ASSERT_FLOAT_IS_DETERMINATE(a);
    printf("voltage= %f\n", a);
    Adc3::deinit();
}
