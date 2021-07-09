#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "Pin.h"
#include "Spi.h"
static int sendSPI(SPI *spi, Pin& cs, uint8_t *b, int cnt, uint8_t *r)
{
    cs.set(false);
    for (int i = 0; i < cnt; ++i) {
        r[i] = spi->write(b[i]);
    }
    cs.set(true);
    return cnt;
}

REGISTER_TEST(SPITest, Spi_class)
{
    SPI *spi = SPI::getInstance(0);
    TEST_ASSERT_NOT_NULL(spi);
    TEST_ASSERT_TRUE(spi->init(8, 0, 100000));
    TEST_ASSERT_TRUE(spi->valid());
    TEST_ASSERT_FALSE(spi->init(8, 1, 100000));

    Pin cs[]= {
    	// led pins on Nucleo
    	Pin("PB0", Pin::AS_OUTPUT),
    	Pin("PE1", Pin::AS_OUTPUT),
    	Pin("PB14", Pin::AS_OUTPUT),
    };

    for(auto& p : cs) {
    	printf("checking cs pin: %s\n", p.to_string().c_str());
    	TEST_ASSERT_TRUE(p.connected());
    	p.set(true);
    }


    for(auto& p : cs) {
		uint32_t data= 0x012345ul;
		uint8_t tx_buf[3]{(uint8_t)(data>>16), (uint8_t)(data>>8), (uint8_t)(data&0xFF)};
		uint8_t rx_buf[3]{0};

    	printf("testing channel with cs pin: %s\n", p.to_string().c_str());
    	int n= sendSPI(spi, p, tx_buf, 3, rx_buf);
    	TEST_ASSERT_EQUAL_INT(3, n);
    	uint32_t tdata= ((tx_buf[0] << 16) | (tx_buf[1] << 8) | (tx_buf[2]));
		printf("  sent: %02X %02X %02X (%08lX)\n", tx_buf[0], tx_buf[1], tx_buf[2], tdata);
		uint32_t rdata= ((rx_buf[0] << 16) | (rx_buf[1] << 8) | (rx_buf[2])) >> 4;
		printf("  rcvd: %02X %02X %02X (%08lX)\n", rx_buf[0], rx_buf[1], rx_buf[2], rdata);
	}
	SPI::deleteInstance(0);
}

