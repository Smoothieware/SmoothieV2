#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


int vcom_read(uint8_t, uint8_t *buf, size_t len);
int vcom_write(uint8_t, uint8_t *buf, size_t len);
void teardown_vcom(uint8_t);
void *setup_vcom(uint8_t);
uint32_t get_dropped_bytes(uint8_t);

#ifdef __cplusplus
}
#endif

