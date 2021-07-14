#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


int vcom_read(uint8_t *buf, size_t len);
int vcom_write(uint8_t *buf, size_t len);
void teardown_vcom();
void setup_vcom();
uint32_t get_dropped_bytes();

#ifdef __cplusplus
}
#endif

