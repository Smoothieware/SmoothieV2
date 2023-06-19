// build with... rake testing=1 test=temperaturecontrol modules=tools/temperaturecontrol -m

#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "Module.h"
#include "TemperatureControl.h"
#include "TempSensor.h"
#include "Dispatcher.h"
#include "OutputStream.h"
#include "SlowTicker.h"
#include "FastTicker.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <memory>
#include <string.h>
#include <sstream>
#include "prettyprint.hpp"
#include <iostream>

const static char test_config[]= "\
[temperature control]\n\
bed.enable = true\n\
bed.sensor = test\n\
bed.runaway_range =  10\n\
bed.runaway_heating_timeout = 2\n\
bed.runaway_cooling_timeout = 3\n\
bed.runaway_error_range = 1.0\n\
";

static float mock_temp= 0;
class MockSensor : public TempSensor
{
    public:
        MockSensor(){};
        virtual ~MockSensor(){};
        float get_temperature() { return mock_temp; }
};

// set the sensor to our mock sensor
static class MockSensor mock_sensor;
static class TemperatureControl *tempcontrol;

REGISTER_TEST(TemperatureControl,setup)
{
    // we need to create and start the slow ticker
    SlowTicker *slt = SlowTicker::getInstance();
    FastTicker *fstt = FastTicker::getInstance();

    // load config with required settings for this test
    std::stringstream ss(test_config);
    ConfigReader cr(ss);
    TEST_ASSERT_TRUE(TemperatureControl::load_controls(cr));

    // check it is there
    Module *m= Module::lookup("temperature control", "bed");
    TEST_ASSERT_NOT_NULL(m);
    tempcontrol= static_cast<TemperatureControl*>(m);

    // set the sensor to our mock sensor
    bool r = tempcontrol->request("set_sensor", (void*)&mock_sensor);
    TEST_ASSERT_TRUE(r);

    // start the timers
    TEST_ASSERT_TRUE(slt->start());
    TEST_ASSERT_TRUE(fstt->start());
}

REGISTER_TEST(TemperatureControl,set_get_temps)
{
    // first make sure we can set and get the target temperature directly
    float t= 123.0F;
    bool r = tempcontrol->request("set_temperature", &t);
    TEST_ASSERT_TRUE(r);

    // set the current temp
    mock_temp = t;

    // give it time to read it
    vTaskDelay(pdMS_TO_TICKS(100));

    // make sure we get it
    TemperatureControl::pad_temperature_t temp;
    TEST_ASSERT_TRUE(tempcontrol->request("get_current_temperature", &temp));
    printf("%s: %s (%d) temp: %f/%f @%d\n", tempcontrol->get_instance_name(), temp.designator.c_str(), temp.tool_id, temp.current_temperature, temp.target_temperature, temp.pwm);
    TEST_ASSERT_EQUAL(t, temp.target_temperature);
    TEST_ASSERT_EQUAL(t, temp.current_temperature);

    // set temp back to 0
    t= 0;
    r = tempcontrol->request("set_temperature", &t);
    TEST_ASSERT_TRUE(r);
}

REGISTER_TEST(TemperatureControl,change_temp_down)
{
    float t= 100.0F;
    bool r = tempcontrol->request("set_temperature", &t);
    TEST_ASSERT_TRUE(r);

    printf("Set temp to 100\n");
    // set the current temp
    mock_temp = t;

    // give it time to test failsafe
    vTaskDelay(pdMS_TO_TICKS(4000));

    // set new temp much lower
    printf("Set temp to 60 but leave current temp at 100\n");
    float t2= 60.0F;
    r = tempcontrol->request("set_temperature", &t2);
    TEST_ASSERT_TRUE(r);

    // give it time to test failsafe
    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("Set temp to 60\n");
    mock_temp = 60;

    // give it time to test failsafe
    vTaskDelay(pdMS_TO_TICKS(10000));

}

