#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"

extern USBD_CDC_ItfTypeDef  USBD_CDC_fops;

size_t vcom_read(uint8_t *buf, size_t len);
int vcom_write(uint8_t *buf, size_t len);
void setup_vcom();
void teardown_vcom();

#ifdef __cplusplus
}
#endif

