#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    size_t size;
    uint8_t *buffer;
    size_t rindex, windex;
} RingBuffer_t;

#ifdef __cplusplus
 extern "C" {
#endif

RingBuffer_t *CreateRingBuffer(size_t size);
void DeleteRingBuffer(RingBuffer_t *r);
bool RingBufferEmpty(RingBuffer_t *r);
bool RingBufferFull(RingBuffer_t *r);
bool RingBufferPut(RingBuffer_t *r, uint8_t value);
bool RingBufferGet(RingBuffer_t *r, uint8_t *value);

#ifdef __cplusplus
 }
#endif
