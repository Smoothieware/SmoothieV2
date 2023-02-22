#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "benchmark_timer.h"

#include "QuadEncoder.h"
#include "RotaryEncoder.h"
#include "Pin.h"

using systime_t = uint32_t;

#define PPR 2000

static float calc_encoder_delta(uint32_t last_cnt, uint32_t cnt)
{
    float delta= 0;
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t qediv = get_quadrature_encoder_div();

    int sign= 1;

    // handle encoder wrap around and get encoder pulses since last read
    if(cnt < last_cnt && (last_cnt - cnt) > (qemax / 2)) {
        delta = (qemax - last_cnt) + cnt + 1;
        sign= 1;
    } else if(cnt > last_cnt && (cnt - last_cnt) > (qemax / 2)) {
        delta = (qemax - cnt) + last_cnt + 1;
        sign= -1;
    } else if(cnt > last_cnt) {
        delta = cnt - last_cnt;
        sign = 1;
     }else if(cnt < last_cnt){
        delta = last_cnt - cnt;
        sign = -1;
    }
    last_cnt = cnt;

    return (sign * delta) / qediv;
}

static float get_encoder_delta()
{
    static uint32_t last_cnt = 0;
    uint32_t cnt = read_quadrature_encoder();
    float d= calc_encoder_delta(last_cnt, cnt);
    last_cnt= cnt;
    return d;
}

static float handle_rpm()
{
    static uint32_t last = 0;
    float rpm;
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t qediv = get_quadrature_encoder_div();
    uint32_t cnt = read_quadrature_encoder();
    uint32_t c = (cnt > last) ? cnt - last : last - cnt;
    last = cnt;

    // deal with over/underflow
    if(c > qemax / 2 ) {
        c = qemax - c + 1;
    }

    rpm = (c * 60.0F * 10) / (PPR * qediv);

    return rpm;
}

// return true if a and b are within the delta range of each other
static bool test_float_within(float a, float b, float delta)
{
    float diff = a - b;
    if (diff < 0) diff = -diff;
    if (delta < 0) delta = -delta;
    printf("diff= %f, delta= %f\n", diff, delta);
    return (diff <= delta);
}

REGISTER_TEST(QETest, float_within)
{
    float delta_mm = roundf((1.0F / 400.0F) * 10000.0F) / 10000.0F;
    printf("delta_mm= %f\n", delta_mm);
    TEST_ASSERT_TRUE(test_float_within(1.0F, 1.0020F, delta_mm));
    TEST_ASSERT_TRUE(test_float_within(1.0F, 1.0024F, delta_mm));
    TEST_ASSERT_TRUE(test_float_within(1.0F, 1.00245F, delta_mm));
    //TEST_ASSERT_TRUE(test_float_within(1.0F, 1.0025F, delta_mm));
    TEST_ASSERT_FALSE(test_float_within(1.0F, 1.0026F, delta_mm));
    TEST_ASSERT_FALSE(test_float_within(1.0F, 10.0F, delta_mm));

    TEST_ASSERT_TRUE(test_float_within(-1.0F, -1.0020F, delta_mm));
    TEST_ASSERT_TRUE(test_float_within(-1.0F, -1.0024F, delta_mm));
    TEST_ASSERT_TRUE(test_float_within(-1.0F, -1.00245F, delta_mm));
    //TEST_ASSERT_TRUE(test_float_within(1.0F, 1.0025F, delta_mm));
    TEST_ASSERT_FALSE(test_float_within(-1.0F, -1.0026F, delta_mm));
    TEST_ASSERT_FALSE(test_float_within(-1.0F, -10.0F, delta_mm));
}

#if 0
REGISTER_TEST(QETest, delta)
{
    TEST_ASSERT_EQUAL_FLOAT(0.25, calc_encoder_delta(0, 1));
    TEST_ASSERT_EQUAL_FLOAT(-0.25, calc_encoder_delta(1, 0));
    TEST_ASSERT_EQUAL_FLOAT(-0.25, calc_encoder_delta(0, 65535));
    TEST_ASSERT_EQUAL_FLOAT(0.25, calc_encoder_delta(65535, 0));

    TEST_ASSERT_EQUAL_FLOAT(0.5, calc_encoder_delta(1, 3));
    TEST_ASSERT_EQUAL_FLOAT(-0.5, calc_encoder_delta(3, 1));
    TEST_ASSERT_EQUAL_FLOAT(-0.5, calc_encoder_delta(1, 65535));
    TEST_ASSERT_EQUAL_FLOAT(0.5, calc_encoder_delta(65535, 1));

    TEST_ASSERT_EQUAL_FLOAT(1.0, calc_encoder_delta(1, 5));
    TEST_ASSERT_EQUAL_FLOAT(-1.0, calc_encoder_delta(5, 1));
    TEST_ASSERT_EQUAL_FLOAT(-1.0, calc_encoder_delta(1, 65533));
    TEST_ASSERT_EQUAL_FLOAT(1.0, calc_encoder_delta(65533, 1));
}
#endif

// Tests both hardware encoder and S/W rotary encoder
REGISTER_TEST(QETest, basic_read)
{
    TEST_ASSERT_TRUE(setup_quadrature_encoder());

    // pin1 and pin2 must be interrupt capable pins that have not already got interrupts assigned for that line number
    Pin pin1("PJ6^"), pin2("PJ9^");
    RotaryEncoder enc(pin1, pin2);
    TEST_ASSERT_TRUE(enc.setup());

    uint32_t last_cnt = 0;
    uint32_t last_rotary_cnt= 0;

    while(1) {
        uint32_t cnt = read_quadrature_encoder();

        if(last_cnt != cnt) {
            printf("qe: raw %ld - diff %ld - delta %f - rpm %f\n", cnt, cnt-last_cnt, get_encoder_delta(), handle_rpm());
            last_cnt = cnt;
        }

        uint32_t c= enc.get_count();

        if(last_rotary_cnt != c) {
            printf("Rotary count= %lu - diff %ld\n", c, c-last_rotary_cnt);
            last_rotary_cnt= c;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
