#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"
#include "FreeRTOS.h"
#include "task.h"

#include "Pwm.h"

REGISTER_TEST(PWMTest, basic_test)
{
    printf("Setting PWM1 frequency to 20KHz\n");
    TEST_ASSERT_TRUE(Pwm::setup(0, 20000));

    Pwm pwm1("PWM1_1");
    Pwm pwmx("X1.2");
    Pwm pwm2("PWM1_2");
    Pwm pwm3("PWM2_1");

    TEST_ASSERT_TRUE(pwm1.is_valid());
    TEST_ASSERT_TRUE(pwm2.is_valid());
    TEST_ASSERT_FALSE(pwmx.is_valid());
    TEST_ASSERT_FALSE(pwm3.is_valid());

    TEST_ASSERT_TRUE(Pwm::post_config_setup());

    TEST_ASSERT_EQUAL_FLOAT(0, pwm1.get());
    TEST_ASSERT_EQUAL_FLOAT(0, pwm2.get());

    printf("Setting channel 1 to 50%% and channel 2 to 25%%\n");
    pwm1.set(0.5F);
    pwm2.set(0.25F);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, pwm1.get());
    TEST_ASSERT_EQUAL_FLOAT(0.25F, pwm2.get());
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Setting channel 1 to 25%% and channel 2 to 0%%\n");
    pwm1.set(0.25);
    pwm2.set(0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Setting channel 1 to 75%%\n");
    pwm1.set(0.75);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

