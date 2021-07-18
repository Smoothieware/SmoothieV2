/**
  *
  * Modified from....
  * Copyright (c) 2018 Benedek Kupper
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

#include "vcom_if.h"
#include "usbd_cdc.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "stream_buffer.h"

static void vcom_if_open      (void* itf, USBD_CDC_LineCodingType * lc);
static void vcom_if_in_cmplt  (void* itf, uint8_t * pbuf, uint16_t length);
static void vcom_if_out_cmplt (void* itf, uint8_t * pbuf, uint16_t length);
static void vcom_set_connected(void* itf, uint8_t dtr, uint8_t rts);

typedef struct {
	uint8_t rx_buffer[USB_EP0_FS_MAX_PACKET_SIZE];
	uint8_t tx_buffer[USB_EP0_FS_MAX_PACKET_SIZE];
	StreamBufferHandle_t xStreamBuffer;
	uint32_t dropped_bytes;
	uint8_t tx_complete;
	uint8_t rx_full;
	uint8_t connected;
} VCOM_STATE_T;

static VCOM_STATE_T *vcom_states[2];
static void *vcom_ifs[2];

static const USBD_CDC_AppType vcom_app = {
	.Name           = "VCOM port",
	.Open           = vcom_if_open,
	.Received       = vcom_if_out_cmplt,
	.Transmitted    = vcom_if_in_cmplt,
	.SetCtrlLine    = vcom_set_connected
};

void *setup_vcom(uint8_t i)
{
	if(i >= 2 || vcom_states[i] != NULL) return NULL;
	VCOM_STATE_T *state= malloc(sizeof(VCOM_STATE_T));
	if(state == NULL) {
		printf("ERROR: VCOM no mem\n");
		return NULL;
	}
	memset(state, 0, sizeof(VCOM_STATE_T));
	vcom_states[i]= state;
	state->xStreamBuffer = xStreamBufferCreate(1024 * 2, 1);
	state->tx_complete = 1;

	USBD_CDC_IfHandleType *vcom_if= malloc(sizeof(USBD_CDC_IfHandleType));
	if(vcom_if == NULL) {
		printf("ERROR: VCOM no mem\n");
		return NULL;
	}
	memset(vcom_if, 0, sizeof(USBD_CDC_IfHandleType));
	vcom_if->App = &vcom_app;
	vcom_if->Base.AltCount = 1;
	vcom_if->instance = i;
	vcom_ifs[i]= vcom_if;
	printf("DEBUG: VCOM %d setup\n", i+1);
	return vcom_if;
}

void teardown_vcom(uint8_t i)
{
	if(i >= 2 || vcom_states[i] == NULL) return;
	char buf[1] = {0};
	xStreamBufferSend(vcom_states[i]->xStreamBuffer, (void *)buf, 1, 1000);
	vStreamBufferDelete(vcom_states[i]->xStreamBuffer);
	free(vcom_states[i]);
	vcom_states[i]= NULL;
	free(vcom_ifs[i]);
	vcom_ifs[i]= NULL;
	printf("DEBUG: VCOM %d teardown\n", i+1);
}

uint32_t vcom_get_dropped_bytes(uint8_t i)
{
	if(i >= 2 || vcom_states[i] == NULL) return 0;
	uint32_t db = vcom_states[i]->dropped_bytes;
	vcom_states[i]->dropped_bytes = 0;
	return db;
}

static void vcom_set_connected(void* itf, uint8_t dtr, uint8_t rts)
{
	uint8_t i= ((USBD_CDC_IfHandleType*)itf)->instance;
	if(i >= 2 || vcom_states[i] == NULL) {
		printf("ERROR: vcom_set_connected illegal instance: %d\n", i);
		return;
	}
	//printf("DEBUG: vcom_set_connected: %d - dtr:%d, rts:%d\n", i, dtr, rts);
	if(vcom_states[i]->connected == 0) {
		// send one byte to indicate we are connected
		vcom_states[i]->connected = 1;
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		char buf[1] = {1};
		xStreamBufferSendFromISR(vcom_states[i]->xStreamBuffer, (void *)buf, 1, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

int vcom_is_connected(uint8_t i)
{
	if(i >= 2 || vcom_states[i] == NULL) return 0;
    return vcom_states[i]->connected;
}

// gets called when the LineCoding gets set
static void vcom_if_open(void* itf, USBD_CDC_LineCodingType * lc)
{
	uint8_t i= ((USBD_CDC_IfHandleType*)itf)->instance;
	if(i >= 2 || vcom_states[i] == NULL) {
		printf("ERROR: vcom_if_open illegal instance: %d\n", i);
		return;
	}
	USBD_CDC_Receive(vcom_ifs[i], vcom_states[i]->rx_buffer, sizeof(vcom_states[i]->rx_buffer));
}

// called when previous transmit completes
static void vcom_if_in_cmplt(void* itf, uint8_t * pbuf, uint16_t length)
{
	uint8_t i= ((USBD_CDC_IfHandleType*)itf)->instance;
	if(i >= 2 || vcom_states[i] == NULL) {
		return;
	}
	vcom_states[i]->tx_complete= 1;
}

// return bytes written, or -1 on error
int vcom_write(uint8_t i, uint8_t *buf, size_t len)
{
	if(i >= 2 || vcom_states[i] == NULL) return -1;
	if(vcom_states[i]->connected == 0) return -1;

    if(vcom_states[i]->tx_complete == 0) {
        // still busy from last write
        return 0;
    }

    vcom_states[i]->tx_complete = 0;
    size_t n = (len > sizeof(vcom_states[i]->tx_buffer)) ? sizeof(vcom_states[i]->tx_buffer) : len;
    memcpy(vcom_states[i]->tx_buffer, buf, n);

    // we transfer it to the tx_buffer as the USB can access SRAM1 faster than AXI_SRAM
    // However it does not appear to be using DMA so not sure if this is valid.
	if(USBD_CDC_Transmit(vcom_ifs[i], vcom_states[i]->tx_buffer, n) != USBD_E_OK){
		return -1;
	}

    return n;
}

// called when incoming data is recieved
static void vcom_if_out_cmplt(void *itf, uint8_t *pbuf, uint16_t length)
{
	uint8_t i= ((USBD_CDC_IfHandleType*)itf)->instance;
	if(i >= 2 || vcom_states[i] == NULL) {
		printf("ERROR: vcom_if_out_cmplt illegal instance: %d\n", i);
		return;
	}
    // we have a buffer from Host, stick it in the stream buffer
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.
    size_t xBytesSent = xStreamBufferSendFromISR(vcom_states[i]->xStreamBuffer, (void *)pbuf, length, &xHigherPriorityTaskWoken);

    if(xBytesSent != length) {
        // There was not enough free space in the stream buffer for the entire buffer
        vcom_states[i]->dropped_bytes += (length - xBytesSent);
    }

    // if we have enough room schedule another read
    if(xStreamBufferSpacesAvailable(vcom_states[i]->xStreamBuffer) >= sizeof(vcom_states[i]->rx_buffer)) {
        // allow more data to be sent
        USBD_CDC_Receive(vcom_ifs[i], vcom_states[i]->rx_buffer, sizeof(vcom_states[i]->rx_buffer));
    }else{
        vcom_states[i]->rx_full= 1;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int vcom_read(uint8_t i, uint8_t *buf, size_t len)
{
	if(i >= 2 || vcom_states[i] == NULL) return -1;
    const TickType_t xBlockTime = portMAX_DELAY;

    // Receive up to another len bytes from the stream buffer.
    // Wait in the Blocked state (so not using any CPU processing time) for at least 1 byte to be available.
    size_t cnt= xStreamBufferReceive(vcom_states[i]->xStreamBuffer,(void *)buf, len, xBlockTime);
    // if we got some data but buffer was full and we have enough for another packet then schedule another read
    if(cnt > 0 && vcom_states[i]->rx_full && xStreamBufferSpacesAvailable(vcom_states[i]->xStreamBuffer) >= sizeof(vcom_states[i]->rx_buffer)) {
        // allow more data to be sent
        vcom_states[i]->rx_full= 0;
        USBD_CDC_Receive(vcom_ifs[i], vcom_states[i]->rx_buffer, sizeof(vcom_states[i]->rx_buffer));
        // printf("WARNING: buffer was full\n");
    }
    return cnt;
}
