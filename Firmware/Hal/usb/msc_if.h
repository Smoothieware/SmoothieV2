#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void *setup_msc();
int STORAGE_IsReady(uint8_t lun);
int STORAGE_Read(uint8_t lun, uint8_t * buf, uint32_t blk_addr, uint16_t blk_len);

#ifdef __cplusplus
}
#endif
