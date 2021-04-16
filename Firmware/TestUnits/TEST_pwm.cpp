#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "Pwm.h"
REGISTER_TEST(PWMTest, from_string)
{
    TEST_ASSERT_TRUE(Pwm::setup(10000));

    Pwm pwm1;
    TEST_ASSERT_FALSE(pwm1.is_valid());
    TEST_ASSERT_FALSE(pwm1.from_string("X1.2"));
    TEST_ASSERT_FALSE(pwm1.is_valid());

    Pwm pwm2;
    TEST_ASSERT_TRUE(pwm2.from_string("P2.12"));
    TEST_ASSERT_TRUE(pwm2.is_valid());

    Pwm pwm3("P2.11");
    TEST_ASSERT_TRUE(pwm3.is_valid());
}


// current = dutycycle * 2.0625
REGISTER_TEST(PWMTest, set_current)
{
    // set X driver to 400mA
    // set Y driver to 1amp
    // set Z driver to 1.5amp
    Pwm pwmx("P7.4"); // X
    TEST_ASSERT_TRUE(pwmx.is_valid());
    // dutycycle= current/2.0625
    float dcp= 0.4F/2.0625F;
    pwmx.set(dcp);

    Pwm pwmy("PB.2"); // Y
    TEST_ASSERT_TRUE(pwmy.is_valid());

    // dutycycle= current/2.0625
    dcp= 1.0F/2.0625F;
    pwmy.set(dcp);

    Pwm pwmz("PB.3"); // Z
    TEST_ASSERT_TRUE(pwmz.is_valid());
    // dutycycle= current/2.0625
    dcp= 1.5F/2.0625F;
    pwmz.set(dcp);
}

