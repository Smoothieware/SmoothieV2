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
    vTaskDelay(pdMS_TO_TICKS(32 * 10 * 2 + 100));

    uint16_t min=0xFFFF, max= 0;
    for (int i = 0; i < 10; ++i) {
        uint16_t v = adc2->read();
        float volts = 3.3F * (v / (float)max_adc_value);
        printf("adc2= %04X, volts= %10.4f\n", v, volts);

        v = adc1->read();
        volts = 3.3F * (v / (float)max_adc_value);
        printf("adc1= %04X, volts= %10.4f\n", v, volts);
        if(v < min) min= v;
        if(v > max) max= v;
        vTaskDelay(pdMS_TO_TICKS(20));
        // takes 2461us between samples for 2 channels
        //printf("time= %lu us\n", adc_get_time());
    }
    printf("Min= %d, Max= %d, spread= %d (%f %%)\n", min, max, max-min, (max-min)*100.0F/(float)max_adc_value);

    TEST_ASSERT_TRUE(adc1->get_errors() == 0);
    TEST_ASSERT_TRUE(adc2->get_errors() == 0);

    TEST_ASSERT_TRUE(Adc::stop());

    Adc::deinit();

    delete adc1;
    delete adc2;
}


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
