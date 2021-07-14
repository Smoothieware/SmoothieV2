#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


int vcom_read(uint8_t *buf, size_t len);
int vcom_write(uint8_t *buf, size_t len);
void teardown_vcom();

#ifdef __cplusplus
}
#endif

