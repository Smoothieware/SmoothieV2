#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

int allocate_hal_pin(void *port, uint16_t pin);
int allocate_hal_interrupt_pin(uint16_t pin, void (*fnc)(void));
#ifdef __cplusplus
}
#endif
