#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "FastTicker.h"

#include "FreeRTOS.h"
#include "task.h"
#include "benchmark_timer.h"

#include "stm32h7xx.h" // for HAL_Delay()

static volatile int timer_cnt20hz= 0;
static volatile int timer_cnt2khz= 0;
static volatile int timer_cnt10khz= 0;

static void timer_callback20hz(void)
{
    ++timer_cnt20hz;
}

static void timer_callback2khz(void)
{
    ++timer_cnt2khz;
}

static void timer_callback10khz(void)
{
    ++timer_cnt10khz;
}

REGISTER_TEST(FastTicker, test_2khz_and_10khz)
{
    // Output HSEClk /4 on MCO1 pin(PA8)
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_4);

    // first test the HAL tick is still running
    uint32_t s = benchmark_timer_start();
    HAL_Delay(10);
    printf("10ms HAL_Delay took: %lu us\n", benchmark_timer_as_us(benchmark_timer_elapsed(s)));

    FastTicker *flt= FastTicker::getInstance();

    TEST_ASSERT_NOT_NULL(flt);
    TEST_ASSERT_FALSE(flt->is_running());

    // won't start as nothing has attached to it
    TEST_ASSERT_FALSE(flt->start());

    // 20 Hz
    int n0= flt->attach(20, timer_callback20hz);
    TEST_ASSERT_TRUE(n0 >= 0);

    // 2 KHz
    int n1= flt->attach(2000, timer_callback2khz);
    TEST_ASSERT_TRUE(n1 >= 0);

    // start it now
    TEST_ASSERT_TRUE(flt->start());
    TEST_ASSERT_TRUE(flt->is_running());

    // test for 2 seconds
    timer_cnt20hz= 0;
    timer_cnt2khz= 0;
    HAL_Delay(2000);
    printf("time 2 seconds, timer2khz %d, timer20hz %d\n", timer_cnt2khz, timer_cnt20hz);
    TEST_ASSERT_INT_WITHIN(1, 2*20, timer_cnt20hz);
    TEST_ASSERT_INT_WITHIN(10, 2*2000, timer_cnt2khz);

    // add 10 KHz
    int n2= flt->attach(10000, timer_callback10khz);
    TEST_ASSERT_TRUE(n2 >= 0 && n2 > n1);

    timer_cnt20hz= 0;
    timer_cnt2khz= 0;
    timer_cnt10khz= 0;
    // test for 2 seconds
    HAL_Delay(2000);
    TEST_ASSERT_TRUE(flt->stop());
    printf("time 2 seconds, timer2khz %d, timer10khz %d, timer20hz %d\n", timer_cnt2khz, timer_cnt10khz, timer_cnt20hz);

    flt->detach(n1);
    flt->detach(n2);

    TEST_ASSERT_INT_WITHIN(1, 2*20, timer_cnt20hz);
    TEST_ASSERT_INT_WITHIN(50, 2*2000, timer_cnt2khz);
    TEST_ASSERT_INT_WITHIN(200, 2*10000, timer_cnt10khz);

    FastTicker::deleteInstance();
}
