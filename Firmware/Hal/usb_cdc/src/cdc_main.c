#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "stm32h7xx_hal.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

USBD_HandleTypeDef USBD_Device;
extern PCD_HandleTypeDef g_hpcd;
extern USBD_CDC_ItfTypeDef USBD_CDC_fops;

static xTaskHandle xTaskToNotify = NULL;


void vcom_notify_recvd()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if(xTaskToNotify != NULL) {
        // Notify the task that data is available
        vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

// called externally to read/write to the USB CDC channel
// Expects entire buffer to be fully written
// vcom_write will copy the buffer or as much of it as it can
// so we need to stay here until all requested data has been transfered to the buffer
size_t write_cdc(const char *buf, size_t len)
{
    size_t sent = 0;
    while(sent < len) {
        int n= vcom_write((uint8_t *)buf + sent, len - sent);
        if(n < 0) {
            // we got an error
            return 0;
        }
        sent += n;
        if(sent < len) {
            // if(!vcom_connected()) return 0; // indicates error
            // yield some time
            taskYIELD();
        }
    }

    return len;
}

size_t read_cdc(char *buf, size_t len)
{
    return vcom_read((uint8_t *)buf, len);
}

int setup_cdc(xTaskHandle h)
{
    /* Store the handle of the calling task. */
    xTaskToNotify = h;

    HAL_PWREx_EnableUSBVoltageDetector();

    setup_vcom();

    int stat;
    // Init Device Library
    if((stat=USBD_Init(&USBD_Device, &VCP_Desc, 0)) != USBD_OK) {
        USBD_ErrLog("Init failed: %d", stat);
        return 0;
    }

    // Add Supported Class
    if((stat=USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS)) != USBD_OK) {
        USBD_ErrLog("RegisterClass failed: %d", stat);
        return 0;
    };

    // Add CDC Interface Class
    if((stat=USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops)) != USBD_OK) {
        USBD_ErrLog("RegisterInterface failed: %d", stat);
        return 0;
    }

    // Start Device Process
    if((stat=USBD_Start(&USBD_Device)) != USBD_OK) {
        USBD_ErrLog("Start failed: %d", stat);
        return 0;
    }

    /*  enable USB interrupts */
    NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(OTG_FS_IRQn);

    return 1;
}

void shutdown_cdc()
{
    USBD_Stop(&USBD_Device);
    HAL_Delay(250);
    USBD_DeInit(&USBD_Device);
    NVIC_DisableIRQ(OTG_FS_IRQn);
    teardown_vcom();
}

#ifdef USE_USB_FS
void OTG_FS_IRQHandler(void)
#else
void OTG_HS_IRQHandler(void)
#endif
{
    HAL_PCD_IRQHandler(&g_hpcd);
}

