#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <string>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ST7920.h" //include the module
#include "ConfigReader.h"
#include "benchmark_timer.h"
//#include "fonts/TomThumb.h"
#include "fonts/Org_01.h"

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

    // default 5x8 font
    lcd.displayString(0, 0, "This is a test", 14);
    lcd.displayString(1, 0, "This is line 2", 14);
    lcd.refresh();

    // Adafruit font, y is bottom not top
    // lcd.setFont(&TomThumb);
    // int inc= 6;
    // int y= 16+inc;
    // lcd.displayAFString(0, y, 1, "Testing AF 3x5 Font"); y+=6;
    lcd.setFont(&Org_01);
    int inc= 6;
    int y= 16+inc;
    lcd.displayAFString(0, y, 1, "Testing Org01 Font"); y+=inc;
    lcd.displayAFString(0, y, 1, "next line"); y+=inc;
    lcd.displayAFString(0, y, 1, "abcdefghijklmnopqrstuvwxyz"); y+=inc;
    lcd.displayAFString(0, y, 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"); y+=inc;
    lcd.displayAFString(0, y, 1, "0123456789 *+-,.");
    lcd.refresh();

    // outline the last line
    lcd.drawHLine(0, 64-8, 128, 1);
    lcd.drawVLine(0, 64-8, 8, 1);
    lcd.drawHLine(0, 63, 128, 1);
    lcd.drawVLine(127, 64-8, 8, 1);
    lcd.refresh();

    int cnt= 0;
    while(true) {
        uint32_t st = benchmark_timer_start();
        while(benchmark_timer_as_ms(benchmark_timer_elapsed(st)) < 1000) ;
        std::string str = "Count: ";
        str.append(std::to_string(++cnt));
        lcd.displayString(7, 0, str.c_str(), str.size());
        lcd.refresh();
    }
}
