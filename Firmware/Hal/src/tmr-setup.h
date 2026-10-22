#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Setup where frequency is in Hz, delay is in microseconds
int steptimer_setup(uint32_t frequency, uint32_t delay, void *mr0handler, void *mr1handler);
void unsteptimer_start();
void steptimer_stop();
void steptimer_change_frequency(uint32_t frequency);
uint32_t steptimer_get_frequency();

// setup where frequency is in Hz
int fasttick_setup(uint32_t frequency, void *timer_handler);
void fasttick_stop();
int fasttick_set_frequency(uint32_t frequency);

// delays amd microseconds count
uint32_t get_microseconds();
uint32_t get_delta_microseconds(uint32_t last);

#ifdef __cplusplus
}
#endif
