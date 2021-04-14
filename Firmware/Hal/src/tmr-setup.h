#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Setup where frequency is in Hz, delay is in microseconds
int steptimer_setup(uint32_t frequency, uint32_t delay, void *mr0handler, void *mr1handler);
void unsteptimer_start();
void steptimer_stop();

// setup where frequency is in Hz
int fasttick_setup(uint32_t frequency, void *timer_handler);
void fasttick_stop();
int fasttick_set_frequency(uint32_t frequency);

#ifdef __cplusplus
}
#endif
