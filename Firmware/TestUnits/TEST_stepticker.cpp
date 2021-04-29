#include "../Unity/src/unity.h"
#include "TestRegistry.h"
#include "tmr-setup.h"
#include "benchmark_timer.h"

#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx.h" // for HAL_Delay()

#include <stdio.h>
#include <stdlib.h>

using systime_t= uint32_t;

#define _ramfunc_  __attribute__ ((section(".ramfunctions"),long_call,noinline))
#define _fast_data_ __attribute__ ((section(".itcm_text")))

_fast_data_ static volatile systime_t unstep_start= 0, unstep_elapsed= 0;
_fast_data_ static volatile uint32_t timer_cnt= 0;

static _ramfunc_ void step_timer_handler()
{
    if(timer_cnt == 100) {
        unsteptimer_start(); // kick off unstep timer
        unstep_start= benchmark_timer_start();
    }
    ++timer_cnt;
}

static _ramfunc_ void unstep_timer_handler()
{
    unstep_elapsed= benchmark_timer_elapsed(unstep_start);
}

#define FREQUENCY 200000 // 200KHz
#define PULSE     2 // 2us
REGISTER_TEST(STEPTMRTest, test_200Khz)
{
    // first test the HAL tick is still running
    uint32_t s = benchmark_timer_start();
    HAL_Delay(10);
    printf("10ms HAL_Delay took: %lu us\n", benchmark_timer_as_us(benchmark_timer_elapsed(s)));

    // this interrupt should continue to run regardless of RTOS being in critical condition
    taskENTER_CRITICAL();
    /* Start the timer 200KHz, with 1us delay */
    int status = steptimer_setup(FREQUENCY, PULSE, (void *)step_timer_handler, (void *)unstep_timer_handler);
    if(status == 0) {
        printf("ERROR: steptimer setup failed\n");
        TEST_FAIL();
    }

    // wait for 200,000 ticks
    timer_cnt= 0;
    systime_t t1= benchmark_timer_start();
    while(timer_cnt < FREQUENCY) ;
    systime_t elapsed= benchmark_timer_as_us(benchmark_timer_elapsed(t1));

    /* Stop the timer */
    steptimer_stop();

    systime_t unstep_time= benchmark_timer_as_us(unstep_elapsed);
    printf("unstep time: %lu us (%lu)\n", unstep_time, unstep_elapsed);

    printf("elapsed time %lu us, period %f us, unstep time %lu us, timer cnt %lu\n", elapsed, (float)elapsed/timer_cnt, unstep_time, timer_cnt);

    TEST_ASSERT_TRUE(unstep_elapsed > 0);
    TEST_ASSERT_INT_WITHIN(1, 1000000/FREQUENCY, elapsed/timer_cnt); // 5us period
    TEST_ASSERT_INT_WITHIN(1, PULSE, unstep_time);
    taskEXIT_CRITICAL();
}

#ifdef BOARD_NUCLEO
// PB8
#define PULSE_PIN                                GPIO_PIN_8
#define PULSE_GPIO_PORT                          GPIOB
#define PULSE_GPIO_CLK_ENABLE()                  __HAL_RCC_GPIOB_CLK_ENABLE()
#define PULSE_GPIO_CLK_DISABLE()                 __HAL_RCC_GPIOB_CLK_DISABLE()
#elif defined(BOARD_DEVEBOX)
// PD9
#define PULSE_PIN                                GPIO_PIN_9
#define PULSE_GPIO_PORT                          GPIOD
#define PULSE_GPIO_CLK_ENABLE()                  __HAL_RCC_GPIOD_CLK_ENABLE()
#define PULSE_GPIO_CLK_DISABLE()                 __HAL_RCC_GPIOD_CLK_DISABLE()
#else
#error not a known board
#endif

static void setup_pin()
{
    PULSE_GPIO_CLK_ENABLE();
    GPIO_InitTypeDef  GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Pin = PULSE_PIN;
    GPIO_InitStruct.Alternate = 0;

    HAL_GPIO_Init(PULSE_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PULSE_GPIO_PORT, PULSE_PIN, GPIO_PIN_RESET);
}

static void teardown_pin()
{
    HAL_GPIO_DeInit(PULSE_GPIO_PORT, PULSE_PIN);
}

static _ramfunc_ void set_pin(int v)
{
    //HAL_GPIO_WritePin(PULSE_GPIO_PORT, PULSE_PIN, v == 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if(v == 1) {
        PULSE_GPIO_PORT->BSRR = PULSE_PIN;
    }else{
        PULSE_GPIO_PORT->BSRR = PULSE_PIN << 16;
    }
}

static _ramfunc_ void step_pulse()
{
    set_pin(1);
    unsteptimer_start(); // kick off unstep timer
}

static _ramfunc_ void unstep_pulse()
{
    set_pin(0);
}

REGISTER_TEST(STEPTMRTest, pulse_test)
{
    setup_pin();

    steptimer_setup(100000, 2, (void *)step_pulse, (void *)unstep_pulse);

    printf("Check pulse is 100KHz, and 2us wide\n");
    vTaskDelay(pdMS_TO_TICKS(10000));
    printf("Done\n");

    steptimer_stop();
    teardown_pin();
}
