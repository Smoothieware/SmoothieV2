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

#define _ramfunc_ // __attribute__ ((section(".ramfunctions"),long_call,noinline))

static volatile systime_t unstep_start= 0, unstep_elapsed= 0;
static volatile uint32_t timer_cnt= 0;

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
#define PULSE     2 // 1us
REGISTER_TEST(STEPTMRTest, test_200Khz)
{
    // first test the HAL tick is still running
    uint32_t s = benchmark_timer_start();
    HAL_Delay(10);
    printf("10ms HAL_Delay took: %lu us\n", benchmark_timer_as_us(benchmark_timer_elapsed(s)));

    // this interrupt should continue to run regardless of RTOS being in critical condition
    taskENTER_CRITICAL();
    /* Start the timer 200KHz, with 1us delay */
    int permod = steptimer_setup(FREQUENCY, PULSE, (void *)step_timer_handler, (void *)unstep_timer_handler);
    if(permod == 0) {
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
