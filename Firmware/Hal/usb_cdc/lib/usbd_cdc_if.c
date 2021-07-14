#include "usbd_cdc_if.h"

#include "FreeRTOS.h"
#include "stream_buffer.h"

static int8_t CDC_IF_Init(void);
static int8_t CDC_IF_DeInit(void);
static int8_t CDC_IF_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_IF_Receive(uint8_t *pbuf, uint32_t *Len);
static int8_t CDC_IF_TransmitCplt(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_CDC_fops = {
    CDC_IF_Init,
    CDC_IF_DeInit,
    CDC_IF_Control,
    CDC_IF_Receive,
    CDC_IF_TransmitCplt
};

USBD_CDC_LineCodingTypeDef linecoding = {
    115200, /* baud rate*/
    0x00,   /* stop bits-1*/
    0x00,   /* parity - none*/
    0x08    /* nb. of bits 8*/
};

/* USB handler declaration */
extern USBD_HandleTypeDef USBD_Device;
extern void vcom_notify_recvd();

static StreamBufferHandle_t xStreamBuffer;
static uint8_t rx_buffer[CDC_DATA_FS_OUT_PACKET_SIZE] __attribute__((section(".usb_data")));
static uint8_t tx_buffer[CDC_DATA_FS_OUT_PACKET_SIZE] __attribute__((section(".usb_data")));
static volatile int tx_complete;
static volatile int rx_full;
static volatile int connected;
static uint32_t dropped_bytes;

void setup_vcom()
{
    dropped_bytes= 0;
    xStreamBuffer = xStreamBufferCreate(1024*2, 1);
}

void teardown_vcom()
{
    char buf[1]= {0};
    xStreamBufferSend(xStreamBuffer, (void *)buf, 1, 1000);
    vStreamBufferDelete(xStreamBuffer);
}

uint32_t get_dropped_bytes()
{
    uint32_t db= dropped_bytes;
    dropped_bytes= 0;
    return db;
}

/**
  * @brief  CDC_IF_Init
  *         Initializes the CDC media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_IF_Init(void)
{
    USBD_CDC_SetRxBuffer(&USBD_Device, rx_buffer);
    tx_complete = 1;
    rx_full= 0;
    connected = 0;
    return (USBD_OK);
}

/**
  * @brief  CDC_IF_DeInit
  *         DeInitializes the CDC media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_IF_DeInit(void)
{
    return (USBD_OK);
}


/**
  * @brief  CDC_IF_Control
  *         Manage the CDC class requests
  * @param  Cmd: Command code
  * @param  Buf: Buffer containing command data (request parameters)
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_IF_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    switch (cmd) {
        case CDC_SEND_ENCAPSULATED_COMMAND:
            /* Add your code here */
            break;

        case CDC_GET_ENCAPSULATED_RESPONSE:
            /* Add your code here */
            break;

        case CDC_SET_COMM_FEATURE:
            /* Add your code here */
            break;

        case CDC_GET_COMM_FEATURE:
            /* Add your code here */
            break;

        case CDC_CLEAR_COMM_FEATURE:
            /* Add your code here */
            break;

        case CDC_SET_LINE_CODING:
            linecoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | \
                                               (pbuf[2] << 16) | (pbuf[3] << 24));
            linecoding.format     = pbuf[4];
            linecoding.paritytype = pbuf[5];
            linecoding.datatype   = pbuf[6];

            /* Add your code here */
            break;

        case CDC_GET_LINE_CODING:
            pbuf[0] = (uint8_t)(linecoding.bitrate);
            pbuf[1] = (uint8_t)(linecoding.bitrate >> 8);
            pbuf[2] = (uint8_t)(linecoding.bitrate >> 16);
            pbuf[3] = (uint8_t)(linecoding.bitrate >> 24);
            pbuf[4] = linecoding.format;
            pbuf[5] = linecoding.paritytype;
            pbuf[6] = linecoding.datatype;

            /* Add your code here */
            break;

        case CDC_SET_CONTROL_LINE_STATE:
            if(connected == 0) {
                // send one byte to indicate we are connected
                connected = 1;
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                char buf[1]= {1};
                xStreamBufferSendFromISR(xStreamBuffer, (void *)buf, 1, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
            break;

        case CDC_SEND_BREAK:
            /* Add your code here */
            break;

        default:
            break;
    }

    return (USBD_OK);
}

/**
  * @brief  CDC_IF_Receive
  *         Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_IF_Receive(uint8_t *buf, uint32_t *len)
{
    // we have a buffer from Host, stick it in the stream buffer
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.
    size_t xBytesSent = xStreamBufferSendFromISR(xStreamBuffer, (void *)buf, *len, &xHigherPriorityTaskWoken);

    if(xBytesSent != *len) {
        // There was not enough free space in the stream buffer for the entire buffer
        dropped_bytes += (*len - xBytesSent);
    }

    // if we have enough room schedule another read
    if(xStreamBufferSpacesAvailable(xStreamBuffer) >= CDC_DATA_FS_OUT_PACKET_SIZE) {
        // allow more data to be sent
        USBD_CDC_ReceivePacket(&USBD_Device);
    }else{
        rx_full= 1;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return (USBD_OK);
}

// blocks until data available or timeout
size_t vcom_read(uint8_t *buf, size_t len)
{
    const TickType_t xBlockTime = portMAX_DELAY;

    // Receive up to another len bytes from the stream buffer.
    // Wait in the Blocked state (so not using any CPU processing time) for at least 1 byte to be available.
    size_t cnt= xStreamBufferReceive(xStreamBuffer,(void *)buf, len, xBlockTime);

    // if we got some data but buffer was full and we have enough for another packet then schedule another read
    if(cnt > 0 && rx_full && xStreamBufferSpacesAvailable(xStreamBuffer) >= CDC_DATA_FS_OUT_PACKET_SIZE) {
        // allow more data to be sent
        rx_full= 0;
        USBD_CDC_ReceivePacket(&USBD_Device);
        // printf("WARNING: buffer was full\n");
    }
    return cnt;
}

/**
  * @brief  CDC_IF_TransmitCplt
  *         Data transmitted callback
  *
  *         @note
  *         This function is IN transfer complete callback used to inform user that
  *         the submitted Data is successfully sent over USB.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_IF_TransmitCplt(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    tx_complete = 1;
    return (0);
}

// return bytes written, or -1 on error
int vcom_write(uint8_t *buf, size_t len)
{
    if(tx_complete == 0) {
        // still busy from last write
        return 0;
    }

    tx_complete = 0;
    size_t n = (len > CDC_DATA_FS_OUT_PACKET_SIZE) ? CDC_DATA_FS_OUT_PACKET_SIZE : len;
    memcpy(tx_buffer, buf, n);

    // we transfer it to the tx_buffer as the USB can access SRAM1 faster than AXI_SRAM
    // However it does not appear to be using DMA so not sure if this is valid.
    USBD_CDC_SetTxBuffer(&USBD_Device, tx_buffer, n);
    if (USBD_CDC_TransmitPacket(&USBD_Device) == USBD_FAIL) {
        return -1;
    }

    return n;
}

int vcom_connected()
{
    return connected;
}
