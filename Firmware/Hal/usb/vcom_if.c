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

static StreamBufferHandle_t xStreamBuffer;
static uint8_t rx_buffer[USB_EP0_FS_MAX_PACKET_SIZE] __attribute__((section(".usb_data")));
static uint8_t tx_buffer[USB_EP0_FS_MAX_PACKET_SIZE] __attribute__((section(".usb_data")));
static volatile int tx_complete;
static volatile int rx_full;
static volatile int connected;
static uint32_t dropped_bytes;

static const USBD_CDC_AppType console_app = {
	.Name           = "VCOM port",
	.Open           = vcom_if_open,
	.Received       = vcom_if_out_cmplt,
	.Transmitted    = vcom_if_in_cmplt,
	.SetCtrlLine    = vcom_set_connected
};

USBD_CDC_IfHandleType _vcom_if = {
	.App = &console_app,
	.Base.AltCount = 1,
}, *const vcom_if = &_vcom_if;

void setup_vcom()
{
	xStreamBuffer = xStreamBufferCreate(1024 * 2, 1);
	dropped_bytes = 0;
	connected = 0;
	tx_complete = 1;
	rx_full = 0;
}

void teardown_vcom()
{
	char buf[1] = {0};
	xStreamBufferSend(xStreamBuffer, (void *)buf, 1, 1000);
	vStreamBufferDelete(xStreamBuffer);
}

uint32_t get_dropped_bytes()
{
	uint32_t db = dropped_bytes;
	dropped_bytes = 0;
	return db;
}

static void vcom_set_connected(void* itf, uint8_t dtr, uint8_t rts)
{
	printf("DEBUG: vcom_set_connected - dtr:%d, rts:%d\n", dtr, rts);
	if(connected == 0) {
		// send one byte to indicate we are connected
		connected = 1;
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		char buf[1] = {1};
		xStreamBufferSendFromISR(xStreamBuffer, (void *)buf, 1, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

int vcom_is_connected()
{
    return connected;
}

// gets called when the LineCoding gets set
static void vcom_if_open(void* itf, USBD_CDC_LineCodingType * lc)
{
	USBD_CDC_Receive(vcom_if, rx_buffer, sizeof(rx_buffer));
}

// called when previous transmit completes
static void vcom_if_in_cmplt(void* itf, uint8_t * pbuf, uint16_t length)
{
	tx_complete= 1;
}

// return bytes written, or -1 on error
int vcom_write(uint8_t *buf, size_t len)
{
	if(connected == 0) return -1;

    if(tx_complete == 0) {
        // still busy from last write
        return 0;
    }

    tx_complete = 0;
    size_t n = (len > sizeof(tx_buffer)) ? sizeof(tx_buffer) : len;
    memcpy(tx_buffer, buf, n);

    // we transfer it to the tx_buffer as the USB can access SRAM1 faster than AXI_SRAM
    // However it does not appear to be using DMA so not sure if this is valid.
	if(USBD_CDC_Transmit(vcom_if, tx_buffer, n) != USBD_E_OK){
		return -1;
	}

    return n;
}

// called when incoming data is recieved
static void vcom_if_out_cmplt(void *itf, uint8_t *pbuf, uint16_t length)
{
    // we have a buffer from Host, stick it in the stream buffer
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.
    size_t xBytesSent = xStreamBufferSendFromISR(xStreamBuffer, (void *)pbuf, length, &xHigherPriorityTaskWoken);

    if(xBytesSent != length) {
        // There was not enough free space in the stream buffer for the entire buffer
        dropped_bytes += (length - xBytesSent);
    }

    // if we have enough room schedule another read
    if(xStreamBufferSpacesAvailable(xStreamBuffer) >= sizeof(rx_buffer)) {
        // allow more data to be sent
        USBD_CDC_Receive(vcom_if, rx_buffer, sizeof(rx_buffer));
    }else{
        rx_full= 1;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int vcom_read(uint8_t *buf, size_t len)
{
    const TickType_t xBlockTime = portMAX_DELAY;

    // Receive up to another len bytes from the stream buffer.
    // Wait in the Blocked state (so not using any CPU processing time) for at least 1 byte to be available.
    size_t cnt= xStreamBufferReceive(xStreamBuffer,(void *)buf, len, xBlockTime);

    // if we got some data but buffer was full and we have enough for another packet then schedule another read
    if(cnt > 0 && rx_full && xStreamBufferSpacesAvailable(xStreamBuffer) >= sizeof(rx_buffer)) {
        // allow more data to be sent
        rx_full= 0;
        USBD_CDC_Receive(vcom_if, rx_buffer, sizeof(rx_buffer));
        // printf("WARNING: buffer was full\n");
    }
    return cnt;
}
