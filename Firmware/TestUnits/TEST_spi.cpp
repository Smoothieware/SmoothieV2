#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "Pin.h"
#include "Spi.h"

static uint32_t unpack(uint8_t rxbuf[])
{
    return ((rxbuf[0] << 16) | (rxbuf[1] << 8) | (rxbuf[2])) >> 4;
}

static uint8_t *pack(uint32_t data)
{
    static uint8_t txbuf[3];
    txbuf[0]= (uint8_t)(data >> 16);
    txbuf[1]= (uint8_t)(data >>  8);
    txbuf[2]= (uint8_t)(data & 0xff);
    return txbuf;
}

REGISTER_TEST(SPITest, Spi_class)
{
    SPI *spi = SPI::getInstance(0);
    TEST_ASSERT_NOT_NULL(spi);
    TEST_ASSERT_TRUE(spi->init(8, 3, 100000));
    TEST_ASSERT_TRUE(spi->valid());

    printf("Expect an error...\n");
    TEST_ASSERT_FALSE(spi->init(8, 1, 1000000));

    Pin cs[]= {
        Pin("PE0", Pin::AS_OUTPUT),
        Pin("PE1", Pin::AS_OUTPUT),
        Pin("PE4", Pin::AS_OUTPUT),
        Pin("PE5", Pin::AS_OUTPUT)
    };

    for(auto& p : cs) {
        TEST_ASSERT_TRUE(p.connected());
        p.set(true);
        printf("checking cs pin and setting high: %s\n", p.to_string().c_str());
    }

    cs[0].set(false);
    printf("testing channel with cs pin: %s\n", cs[0].to_string().c_str());
    uint32_t data= 0x123400;
    uint32_t lastdata;
    // this tests the passthru/loopback of the TMC chip
    // basically every 20bits is sent back out on the MISO delayed by 20 bits, it ignores extra bits
    // so we send 24 bits but only the last 20 bits are sent out.
    for (uint32_t i = 0; i < 32768; ++i) {
 		uint8_t *tx_buf= pack(data);
		uint8_t rx_buf[3];

        bool stat= spi->write_read(tx_buf, rx_buf, 3);
    	TEST_ASSERT_TRUE(stat);
		// printf("  sent[%ld]: %02X %02X %02X: %05lX\n",i,  tx_buf[0], tx_buf[1], tx_buf[2], data);
		// printf("  rcvd[%ld]: %02X %02X %02X: %05lX\n", i, rx_buf[0], rx_buf[1], rx_buf[2], unpack(rx_buf));
        if(i > 0) TEST_ASSERT_EQUAL_HEX32(lastdata&0x00FFFFF, unpack(rx_buf));
        lastdata= data;
        data += 1;
    }
    cs[0].set(true);

	SPI::deleteInstance(0);
}

