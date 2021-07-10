#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"

extern "C" bool setup_sdmmc();

extern "C" bool qspi_flash(const char *);

static FATFS fatfs; /* File system object */
static const char *fn= "/sd/qspitest.bin";
static const int n= 256;
static uint8_t test_buf[n];

static bool write_test_file()
{
    TEST_ASSERT_EQUAL_INT(1, setup_sdmmc());
    TEST_ASSERT_EQUAL_INT(FR_OK, f_mount(&fatfs, "sd", 1));
    int ret;

    // delete it if it was there
    ret = f_unlink(fn);
    //TEST_ASSERT_EQUAL_INT(FR_OK, ret);

    FIL fp;  /* File object */
    ret = f_open(&fp, fn, FA_WRITE | FA_CREATE_ALWAYS);
    TEST_ASSERT_EQUAL_INT(FR_OK, ret);

    for (int i = 0; i < n; ++i) {
        test_buf[i]= i;
    }

    unsigned x;
    ret = f_write(&fp, test_buf, n, &x);
    TEST_ASSERT_EQUAL_INT(FR_OK, ret);
    TEST_ASSERT_EQUAL_INT(n, x);
    f_close(&fp);

    return true;
}

REGISTER_TEST(QSPITest, basic_test)
{
    // create a test file
    TEST_ASSERT_TRUE(write_test_file());
    // flash it and leave it mapped to 0x90000000
    TEST_ASSERT_TRUE(qspi_flash(fn));

    // check it
    uint8_t *flash_mem= (uint8_t *)0x90000000;
    for (unsigned i = 0; i < sizeof(test_buf); ++i) {
        TEST_ASSERT_EQUAL_INT(i, flash_mem[i]);
    }
}
