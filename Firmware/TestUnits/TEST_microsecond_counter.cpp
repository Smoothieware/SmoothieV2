#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"
#include "benchmark_timer.h"

#include "stm32h7xx.h" // for HAL_Delay()

#include "tmr-setup.h"

REGISTER_TEST(MicroSecondCounter, test_it_is_1us)
{
    microsecond_init();
    // first test the HAL tick is still running
    uint32_t s = benchmark_timer_start();
    HAL_Delay(10);
    printf("10ms HAL_Delay took: %lu us\n", benchmark_timer_as_us(benchmark_timer_elapsed(s)));

    uint32_t st = get_microseconds();
    // test for 2 seconds
    HAL_Delay(2000);
    uint32_t dt = get_microseconds() - st;
    printf("time 2 seconds: %lu us\n", dt);
    TEST_ASSERT_INT_WITHIN(500, dt, 2000000);

    dt = get_delta_microsconds(st);
    TEST_ASSERT_INT_WITHIN(500, dt, 2000000);
}
