#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "Uart.h"

REGISTER_TEST(UARTTest, UART_class)
{
    printf("Started at 9600 baud..\n");
    // UART 1 on Nucleo is pins PD8/PD9
    UART *uart = UART::getInstance(1);
    TEST_ASSERT_NOT_NULL(uart);
    UART::settings_t settings= {
        .baudrate= 9600,
        .bits= 8,
        .stop_bits= 1,
        .parity= 0
    };

    TEST_ASSERT_TRUE(uart->init(settings));
    TEST_ASSERT_TRUE(uart->valid());

    printf("Expect an error report...\n");
    UART::settings_t settings2= {
        .baudrate= 2400,
        .bits= 8,
        .stop_bits= 1,
        .parity= 0
    };
    TEST_ASSERT_FALSE(uart->init(settings2));

    uint8_t buf[]= {'1','2','3','4','5','6','7','8','9','\n', '\0'};
    int n= 10;

    printf("writing 200x: %s \n", buf);
    for (int i = 0; i < 200; ++i) {
        TEST_ASSERT_EQUAL_INT(n, uart->write(buf, n));
        //vTaskDelay(pdMS_TO_TICKS(100));
    }

    printf("Enter upto 10 characters (Timeout in 5 seconds)...\n");
    n= uart->read(buf, 10, 5000);
    printf("read returned %d\n", n);
    if(n > 0) {
        printf("data read:\n");
        for (int i = 0; i < n; ++i) {
            printf("%c ", buf[i]);
        }
        printf("\n");
    }

	UART::deleteInstance(1);
}

