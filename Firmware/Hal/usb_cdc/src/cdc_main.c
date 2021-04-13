#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static xTaskHandle xTaskToNotify = NULL;

void vcom_notify_recvd()
{
    BaseType_t xHigherPriorityTaskWoken= pdFALSE;

    // Notify the task that data is available
    vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

// called externally to read/write to the USB CDC channel
// Expects entire buffer to be fully written
// vcom_write will copy the buffer or as much of it as it can
// so we need to stay here until all requested data has been transfered to the buffer
size_t write_cdc(const char *buf, size_t len)
{
    size_t sent= 0;
    while(sent < len) {
        uint32_t n = vcom_write((uint8_t *)buf+sent, len-sent);
        sent += n;
        if(sent < len) {
            if(!vcom_connected()) return 0; // indicates error
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


    /*  enable USB interrupts */
    NVIC_SetPriority(LPC_USB_IRQ, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(LPC_USB_IRQ);

    return 1;
}

void shutdown_cdc()
{
    NVIC_DisableIRQ(LPC_USB_IRQ);
}




