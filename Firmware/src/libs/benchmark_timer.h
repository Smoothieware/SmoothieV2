#pragma once

#include <stdint.h>
#include "stm32h7xx.h"

#ifdef  __cplusplus
extern "C" {
#endif

void benchmark_timer_init(void);
inline uint32_t benchmark_timer_start(void) { return (DWT->CYCCNT); }
inline uint32_t benchmark_timer_elapsed(uint32_t since) { return DWT->CYCCNT - since; }
uint32_t benchmark_timer_as_ms(uint32_t ticks);
uint32_t benchmark_timer_as_us(uint32_t ticks);

#ifdef  __cplusplus
}
#endif
