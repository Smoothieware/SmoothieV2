#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

int allocate_hal_pin(void *port, uint16_t pin);

#ifdef __cplusplus
}
#endif
