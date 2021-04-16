#include "usbd_cdc_if.h"

#include "ringbuffer_c.h"

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

static RingBuffer_t *rx_rb;
static uint8_t rx_buffer[CDC_DATA_HS_OUT_PACKET_SIZE];
static volatile int tx_complete;

void setup_vcom()
{
    rx_rb = CreateRingBuffer(1024);
}

void teardown_vcom()
{
    DeleteRingBuffer(rx_rb);
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
    tx_complete= 1;
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
            /* Add your code here */
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
extern void vcom_notify_recvd();
static int8_t CDC_IF_Receive(uint8_t *Buf, uint32_t *Len)
{
    // we have a buffer from Host, stick it in the rx buffer and inform thread to read
    for (uint32_t i = 0; i < *Len; ++i) {
        // FIXME: avoid copying 1 byte at a time, use a better ringbuffer or just double buffer
        RingBufferPut(rx_rb, Buf[i]);
    }
    // FIXME: this is really innefficient of gettign 1 byte at a time, need to double buffer so we can interleave
    vcom_notify_recvd();
    return (USBD_OK);
}

size_t vcom_read(uint8_t *buf, size_t len)
{
    size_t cnt = 0;
    while(!RingBufferEmpty(rx_rb) && cnt < len) {
        RingBufferGet(rx_rb, &buf[cnt++]);
    }

    if(cnt > 0 && RingBufferEmpty(rx_rb)) {
        // allow more data to be sent
        USBD_CDC_ReceivePacket(&USBD_Device);
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
    tx_complete= 1;
    return (0);
}

size_t vcom_write(uint8_t *buf, size_t len)
{
    if(tx_complete == 0) return 0;
    tx_complete= 0;
    size_t n= (len > 64) ? 64 : len;

    USBD_CDC_SetTxBuffer(&USBD_Device, buf, n);
    if (USBD_CDC_TransmitPacket(&USBD_Device) == USBD_OK) {
        return n;
    }
    tx_complete= 1;
    return 0;
}
