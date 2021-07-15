//  Simple fixed size ring buffer.
//  Manage objects by value.
//  Thread safe for single Producer and single Consumer.
//  By Dennis Lang http://home.comcast.net/~lang.dennis/code/ring/ring.html
//  rewritten in C

#include "ringbuffer_c.h"

RingBuffer_t *CreateRingBuffer(size_t size)
{
	RingBuffer_t *r= malloc(sizeof(RingBuffer_t));
	r->size= size;
	r->buffer= malloc(size);
	r->rindex= 0;
	r->windex= 0;
	return r;
}

void DeleteRingBuffer(RingBuffer_t *r)
{
	free(r->buffer);
	free(r);
}

bool RingBufferEmpty(RingBuffer_t *r)
{
	return (r->rindex == r->windex);
}

#define next(r, n) ((n + 1) % r->size)

bool RingBufferFull(RingBuffer_t *r)
{
	return (next(r, r->windex) == r->rindex);
}

bool RingBufferPut(RingBuffer_t *r, uint8_t value)
{
	if (RingBufferFull(r)) return false;
	r->buffer[r->windex] = value;
	r->windex = next(r, r->windex);
	return true;
}

bool RingBufferGet(RingBuffer_t *r, uint8_t *value)
{
	if (RingBufferEmpty(r)) return false;
	*value = r->buffer[r->rindex];
	r->rindex = next(r, r->rindex);
	return true;
}

