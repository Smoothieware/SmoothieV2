#include "benchmark_timer.h"
#include "stm32h7xx.h"

extern uint32_t SystemCoreClock;

static uint32_t ticks_per_second;
static uint32_t ticks_per_ms;
static uint32_t ticks_per_us;

/* Initialize benchmark_timer */
void benchmark_timer_init(void)
{
        ticks_per_second = SystemCoreClock;
        ticks_per_ms = ticks_per_second / 1000;
        ticks_per_us = ticks_per_second / 1000000;
#if defined(CoreDebug_DEMCR_TRCENA_Msk)
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;         //enable debug tracer
#endif
        //ITM->LAR = 0xC5ACCE55;                                        //unlock access to dwt, if so equip'd
        DWT->CTRL |= ITM_TCR_ITMENA_Msk;                        //enable dwt cycle count
        DWT->CYCCNT = 0; // reset the counter
}

/* Returns number of ticks per second of the benchmark_timer timer */
uint32_t benchmark_timer_ticks_per_second(void)
{
        return ticks_per_second;
}

/* Converts from benchmark_timer tickbs to ms. */
uint32_t benchmark_timer_as_ms(uint32_t ticks)
{
        return ticks / ticks_per_ms;
}

/* Converts from benchmark_timer ticks to us. */
uint32_t benchmark_timer_as_us(uint32_t ticks)
{
        return ticks / ticks_per_us;
}

/* Converts from benchmark_timer ticks to float us. */
float benchmark_timer_as_ns(uint32_t ticks)
{
        return (float)ticks / ((float)ticks_per_second/1E6F);
}

/* Converts from mS to benchmark_timer ticks. */
uint32_t benchmark_timer_ms_as_ticks(uint32_t ms)
{
        return ms * ticks_per_ms;
}

/* Converts from uS to benchmark_timer ticks. */
uint32_t benchmark_timer_us_as_ticks(uint32_t us)
{
        return us * ticks_per_us;
}



