#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "benchmark_timer.h"

#include "QuadEncoder.h"

using systime_t = uint32_t;

REGISTER_TEST(QETest, basic_read)
{
    TEST_ASSERT_TRUE(setup_quadrature_encoder());

    int32_t last= -1;
    while(1) {
        int32_t cnt = read_quadrature_encoder();
        if(last != cnt) {
            printf("%ld\n", cnt);
            last= cnt;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
