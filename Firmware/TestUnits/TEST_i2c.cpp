#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "i2c.h"

REGISTER_TEST(I2CTest, i2c_write)
{
    I2C *i2c = I2C::getInstance(0);
    TEST_ASSERT_NOT_NULL(i2c);
    TEST_ASSERT_TRUE(i2c->init(400000));
    TEST_ASSERT_TRUE(i2c->valid());
    TEST_ASSERT_FALSE(i2c->init(100000));

	uint8_t tx_buf[4]{0x12, 0x34, 0x56, 0x78};

	bool ok= i2c->write(0x20, tx_buf, 4);
	printf("i2c write returned: %d\n", ok);

	I2C::deleteInstance(0);
}

