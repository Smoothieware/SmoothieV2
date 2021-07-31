#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "Pin.h"
#include "Spi.h"

static bool sendSPI24bits(SPI *spi, Pin& cs, void *b, void *r)
{
    cs.set(false);
    bool stat= spi->write_read(b, r, 3);
    cs.set(true);
    return stat;
}

REGISTER_TEST(SPITest, Spi_class)
{
    SPI *spi = SPI::getInstance(0);
    TEST_ASSERT_NOT_NULL(spi);
    TEST_ASSERT_TRUE(spi->init(8, 3, 1000000));
    TEST_ASSERT_TRUE(spi->valid());

    printf("Expect an error...\n");
    TEST_ASSERT_FALSE(spi->init(8, 1, 1000000));

    Pin cs("PE0", Pin::AS_OUTPUT);

  	printf("checking cs pin: %s\n", cs.to_string().c_str());
  	TEST_ASSERT_TRUE(cs.connected());
   	cs.set(true);

    for (uint8_t i = 0; i < 4; ++i) {
 		uint8_t data[]{0x12, 0x34, (uint8_t)(0x50|i)};
		uint8_t rx_buf[3];

    	printf("testing channel with cs pin: %s\n", cs.to_string().c_str());
    	TEST_ASSERT_TRUE(sendSPI24bits(spi, cs, data, rx_buf));
		printf("  sent: %02X %02X %02X\n", data[0], data[1], data[2]);
		printf("  rcvd: %02X %02X %02X\n", rx_buf[0], rx_buf[1], rx_buf[2]);
    }

	SPI::deleteInstance(0);
}

