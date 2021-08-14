#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "SlowTicker.h"
#include "benchmark_timer.h"

#include "FreeRTOS.h"
#include "timers.h"

static volatile int timer_cnt1= 0;
static volatile int timer_cnt20= 0;
static volatile int timer_cnt100= 0;

static void timer_callback1(void)
{
    ++timer_cnt1;
}

static void timer_callback20(void)
{
    ++timer_cnt20;
}

static void timer_callback100(void)
{
    ++timer_cnt100;
}

REGISTER_TEST(SlowTicker, test_1_20_100_hz)
{
    SlowTicker *slt= SlowTicker::getInstance();
    TEST_ASSERT_NOT_NULL(slt);

    // 20 Hz
    int n1= slt->attach(20, timer_callback20);
    TEST_ASSERT_TRUE(n1 >= 0);

    // 100 Hz
    int n2= slt->attach(100, timer_callback100);
    TEST_ASSERT_TRUE(n2 >= 0 && n2 > n1);

    // 1 Hz
    int n3= slt->attach(1, timer_callback1);
    TEST_ASSERT_TRUE(n3 > n2);

    // start the timers
    TEST_ASSERT_TRUE(slt->start());

    timer_cnt1= 0;
    timer_cnt20= 0;
    timer_cnt100= 0;
    // test for 5 seconds which should be around 100 callbacks for 20 and 500 for 100
    for (int i = 0; i < 5; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("time %d seconds, timer1 %d, timer20 %d, timer100 %d\n", i+1, timer_cnt1, timer_cnt20, timer_cnt100);
    }

    // stop the timers
    TEST_ASSERT_TRUE(slt->stop());

    slt->detach(n1);
    slt->detach(n2);
    slt->detach(n3);

    TEST_ASSERT_INT_WITHIN(2, 100, timer_cnt20);
    TEST_ASSERT_INT_WITHIN(2, 500, timer_cnt100);
    TEST_ASSERT_INT_WITHIN(1, 5, timer_cnt1);
}

using systime_t= uint32_t;
#define TICKS2MS( xTicks ) ( (uint32_t) ( ((uint64_t)(xTicks) * 1000) / configTICK_RATE_HZ ) )
REGISTER_TEST(SlowTicker, xTaskGetTickCount)
{
    static TickType_t last_time_check = xTaskGetTickCount();
    int cnt= 0;

    systime_t st = benchmark_timer_start();
    while(cnt < 10) {
        if(TICKS2MS(xTaskGetTickCount() - last_time_check) >= 100) {
            last_time_check = xTaskGetTickCount();
            cnt++;
        }
    }
    systime_t el = benchmark_timer_elapsed(st);
    printf("elapsed time %lu us\n", benchmark_timer_as_us(el));
    TEST_ASSERT_INT_WITHIN(1000, 1000000, benchmark_timer_as_us(el));
}
