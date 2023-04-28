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
    lcd.fillRect(0, 16, 128, 4, 1); // draw horizontal filled bar 4 pixels in height
    lcd.refresh();

    // Adafruit font, y is bottom not top
    // lcd.setFont(&TomThumb);
    // int inc= 6;
    // int y= 20+inc;
    // lcd.displayAFString(0, y, 1, "Testing AF 3x5 Font"); y+=6;

    lcd.setFont(&Org_01);
    int inc= 6;
    int y= 20+inc;
    lcd.displayAFString(0, y, 1, "Testing Org01 Font"); y+=inc;
    lcd.displayAFString(0, y, 1, "abcdefghijklmnopqrstuvwxyz"); y+=inc;
    lcd.displayAFString(0, y, 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"); y+=inc;
    lcd.displayAFString(0, y, 1, "0123456789 *+-,."); y+=inc;
    lcd.refresh();

    // outline an area
    y-=4;
    lcd.drawRect(0, y, 127, 16);
    lcd.refresh();

    y += 7;
    int cnt= 0;
    while(true) {
        uint32_t st = benchmark_timer_start();
        while(benchmark_timer_as_ms(benchmark_timer_elapsed(st)) < 1000) ;
        std::string str = "Count: ";
        str.append(std::to_string(++cnt));
        // get size of string
        int16_t x1, y1;
        uint16_t w, h;
        lcd.getTextBounds(str.c_str(), 2, y+3, &x1, &y1, &w, &h);
        printf("y= %d, size: %d, %d, %d, %d\n", y, x1, y1, w, h);

        lcd.fillRect(x1, y1, w, h, 0); // clear box for text
        lcd.displayAFString(2, y+3, 1, str.c_str());
        lcd.refresh();
    }
}
