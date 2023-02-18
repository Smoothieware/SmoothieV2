#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "benchmark_timer.h"

#include "QuadEncoder.h"

using systime_t = uint32_t;

static float get_encoder_delta()
{
    static int32_t last_cnt= 0;
    float delta;
    int32_t cnt = read_quadrature_encoder();
    int32_t qemax = get_quadrature_encoder_max_count();

    // handle encoder wrap around and get encoder pulses since last read
    if(cnt < last_cnt && (last_cnt - cnt) > (qemax / 2)) {
        delta = (qemax - last_cnt) + cnt + 1;
    } else if(cnt > last_cnt && (cnt - last_cnt) > (qemax / 2)) {
        delta = (qemax - cnt) + 1;
    } else {
        delta = cnt - last_cnt;
    }
    last_cnt = cnt;

    return delta;
}

static float handle_rpm()
{
    static uint32_t last= 0;
    float rpm;
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t cnt = abs(read_quadrature_encoder());
    uint32_t c = (cnt > last) ? cnt - last : last - cnt;
    last = cnt;

    // deal with over/underflow
    if(c > qemax/2 ) {
        c= qemax - c + 1;
    }

    rpm = (c * 60 * 10) / 25;

    return rpm;
}

REGISTER_TEST(QETest, basic_read)
{
    TEST_ASSERT_TRUE(setup_quadrature_encoder());

    int32_t last= -1;
    while(1) {
        int32_t cnt = read_quadrature_encoder();
        if(last != cnt) {
            printf("%ld - delta %f - rpm %f\n", cnt, get_encoder_delta(), handle_rpm());
            last= cnt;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
