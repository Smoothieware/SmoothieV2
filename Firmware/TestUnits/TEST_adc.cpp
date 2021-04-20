#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "Adc.h"
#include "SlowTicker.h"

#include "FreeRTOS.h"
#include "task.h"

#include "benchmark_timer.h"

using systime_t= uint32_t;

#if 0
REGISTER_TEST(ADCTest, adc_class)
{
    TEST_ASSERT_TRUE(Adc::setup());

    Adc *adc = new Adc;
    TEST_ASSERT_TRUE(adc->is_valid());
    TEST_ASSERT_FALSE(adc->connected());
    TEST_ASSERT_FALSE(adc->from_string("nc") == adc);
    TEST_ASSERT_FALSE(adc->connected());
    TEST_ASSERT_TRUE(adc->from_string("ADC0_1") == adc); // ADC0_1/T1
    TEST_ASSERT_TRUE(adc->connected());
    TEST_ASSERT_EQUAL_INT(1, adc->get_channel());

    // check it won't let us create a duplicate
    Adc *dummy= new Adc();
    TEST_ASSERT_TRUE(dummy->from_string("ADC0_1") == nullptr);
    delete dummy;

    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %lu\n", max_adc_value);

    // give it time to accumulate the 32 samples
    vTaskDelay(pdMS_TO_TICKS(32*10*2 + 100));
    // fill up moving average buffer
    adc->read();
    adc->read();
    adc->read();
    adc->read();

    for (int i = 0; i < 10; ++i) {
        uint16_t v= adc->read();
        float volts= 3.3F * (v / (float)max_adc_value);

        printf("adc= %04X, volts= %10.4f\n", v, volts);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    printf("read_voltage() = %10.4f\n", adc->read_voltage());

    delete adc;
    TEST_ASSERT_TRUE(Adc::stop());

    // should be able to create it now
    dummy= new Adc();
    TEST_ASSERT_TRUE(dummy->from_string("ADC0_1") == dummy);
    delete dummy;
}
#endif

REGISTER_TEST(ADCTest, two_adc_channels)
{
    // Use ADC0 and ADC2 on PA4 and PF11
    Adc *adc1 = new Adc(0);
    Adc *adc2 = new Adc(2);

    TEST_ASSERT_TRUE(Adc::post_config_setup());

    TEST_ASSERT_TRUE(adc1->is_valid());
    TEST_ASSERT_EQUAL_INT(0, adc1->get_channel());

    TEST_ASSERT_TRUE(adc2->is_valid());
    TEST_ASSERT_EQUAL_INT(2, adc2->get_channel());

    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %lu\n", max_adc_value);

    // give it time to accumulate the 32 samples
    vTaskDelay(pdMS_TO_TICKS(32*10*2 + 100));

    for (int i = 0; i < 10; ++i) {
        uint16_t v= adc2->read();
        float volts= 3.3F * (v / (float)max_adc_value);
        printf("adc2= %04X, volts= %10.4f\n", v, volts);

        v= adc1->read();
        volts= 3.3F * (v / (float)max_adc_value);
        printf("adc1= %04X, volts= %10.4f\n", v, volts);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    TEST_ASSERT_TRUE(adc1->get_errors() == 0);
    TEST_ASSERT_TRUE(adc2->get_errors() == 0);

    TEST_ASSERT_TRUE(Adc::stop());

    // delete adc1;
    // delete adc2;
}
