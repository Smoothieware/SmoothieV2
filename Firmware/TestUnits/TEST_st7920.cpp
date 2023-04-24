#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <sstream>
#include <iostream>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ST7920.h" //include the module
#include "ConfigReader.h"

ST7920 lcd;

// define config here, this is in the same format they would appear in the config.ini file
const static char st7920_config[]= "\
[st7920]\n\
enable = true \n\
clk = PJ7 \n\
mosi = PE8 \n\
";

REGISTER_TEST(ST7920, run_tests)
{
    // load config with required settings for this test
    std::stringstream ss(st7920_config);
    ConfigReader cr(ss);
    TEST_ASSERT_TRUE(lcd.configure(cr));

    Module *m= Module::lookup("st7920");
    TEST_ASSERT_NOT_NULL(m);

    lcd.initDisplay();
    lcd.clearScreen();
    lcd.refresh();

    lcd.displayString(0, 0, "This is a test", 14);
    lcd.refresh();
}
