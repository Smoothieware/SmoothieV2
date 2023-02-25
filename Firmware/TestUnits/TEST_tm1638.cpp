/*
  Project Name: TM1638
  File: TM1638plus_TEST_Model1.ino
  Description: ST32 demo file library for  TM1638 module(LED & KEY). Model 1
  Carries out series of tests demonstrating STM32 library TM1638plus.

  TESTS:
  TEST 0 Reset
  TEST 1 Brightness
  TEST 2 ASCII display
  TEST 3 Set a single segment
  TEST 4 Hex digits
  TEST 5 Text String with Decimal point
  TEST 6 TEXT + ASCII combo
  TEST 7 Integer Decimal number
  TEST 8 Text String + Float
  TEST 9 Text String + decimal number
  TEST 10 Multiple dots
  TEST 11 Display Overflow
  TEST 12 Scrolling text
  TEST 13 setLED and setLEDs method
  TEST 14 Buttons + LEDS

  modified from...
  Author: Gavin Lyons.
  Created April  2021
  IDE: STM32CubeIDE C++
  URL: https://github.com/gavinlyonsrepo/STM32_projects
*/

#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <sstream>
#include <iostream>

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

#include "TM1638.h" //include the module
#include "ConfigReader.h"

static void doLEDs(uint8_t value);
static void Test0(void);
static void Test1(void);
static void Test2(void);
static void Test3(void);
static void Test4(void);
static void Test5(void);
static void Test6(void);
static void Test7(void);
static void Test8(void);
static void Test9(void);
static void Test10(void);
static void Test11(void);
static void Test12(void);
static void Test13(void);
static void Test14(void);


// Some vars and defines for the tests.
#define myTestDelay  5000
#define myTestDelay1 1000
uint8_t  testcount = 0;

TM1638 tm;

// define config here, this is in the same format they would appear in the config.ini file
const static char tm1638_config[]= "\
[tm1638]\n\
enable = true \n\
clock_pin = PJ9 \n\
data_pin = PJ10 \n\
strobe_pin = PJ6 \n\
";

REGISTER_TEST(TM1638Test, run_tests)
{
    // load config with required settings for this test
    std::stringstream ss(tm1638_config);
    ConfigReader cr(ss);
    TEST_ASSERT_TRUE(tm.configure(cr));

    Module *m= Module::lookup("tm1638");
    TEST_ASSERT_NOT_NULL(m);

    tm.displayBegin();

    //Test 0 reset
    Test0();

    /* Infinite loop */
    while (1) {
        if(testcount >= 14) testcount= 0;
        printf("Test%d...\n", testcount+1);
        switch (++testcount) {
            case 1: Test1(); break; // Brightness
            case 2: Test2(); break; // ASCII display
            case 3: Test3(); break; // Set a single segment
            case 4: Test4(); break; // Hex digits
            case 5: Test5(); break; // Text String with Decimal point
            case 6: Test6(); break; // TEXT + ASCII combo
            case 7: Test7(); break; // Integer Decimal number
            case 8: Test8(); break; // Text String + Float hack
            case 9: Test9(); break; // Text String + decimal number
            case 10: Test10(); break; // Multiple Decimal points
            case 11: Test11(); break; // Display Overflow
            case 12: Test12(); break; // Scrolling text
            case 13: Test13(); break; // setLED and setLEDs
            case 14: Test14(); break; // Buttons + LEDS
        }
    }
}

static void Test0()
{
    // Test 0 reset test
    tm.setLED(0, 1);
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.reset();
}

static void Test1()
{
    // Test 1  Brightness and reset
    for (uint8_t brightness = 0; brightness < 8; brightness++) {
        tm.brightness(brightness);
        tm.displayText("00000000");
        vTaskDelay(pdMS_TO_TICKS(myTestDelay1));
    }
    tm.reset();
    // restore default brightness
    tm.brightness(0x02);
}

static void Test2()
{
    //Test 2 ASCII , display 2.341

    tm.displayASCIIwDot(0, '2');
    tm.displayASCII(1, '3');
    tm.displayASCII(2, '4');
    tm.displayASCII(3, '1');
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.reset();
}

static void Test3()
{
    //TEST 3 single segment (digit position, (dp)gfedcba)
    // (dp)gfedcba =  seven segments positions
    uint8_t pos = 0;
    for (pos = 0 ; pos < 8 ; pos++) {
        tm.display7Seg(pos, 1 << (7 - pos)); // Displays a single seg in (dp)gfedcba) in each  pos 0-7
        vTaskDelay(pdMS_TO_TICKS(myTestDelay1));
    }
}

static void Test4()
{
    // Test 4 Hex digits.
    tm.displayHex(0, 0);
    tm.displayHex(1, 1);
    tm.displayHex(2, 2);
    tm.displayHex(3, 3);
    tm.displayHex(4, 4);
    tm.displayHex(5, 5);
    tm.displayHex(6, 6);
    tm.displayHex(7, 7);
    vTaskDelay(pdMS_TO_TICKS(myTestDelay)); // display 12345678

    tm.displayHex(0, 8);
    tm.displayHex(1, 9);
    tm.displayHex(2, 0x0A);
    tm.displayHex(3, 0x0B);
    tm.displayHex(4, 0x0C);
    tm.displayHex(5, 0x0D);
    tm.displayHex(6, 0x0E);
    tm.displayHex(7, 0x0F);
    vTaskDelay(pdMS_TO_TICKS(myTestDelay)); // display 89ABCDEF
    tm.reset();

    tm.displayHex(1, 0xFE);
    tm.displayHex(7, 0x10);
    vTaskDelay(pdMS_TO_TICKS(myTestDelay)); // display " E      0"

}

static void Test5()
{
    // Test 5 TEXT  with dec point
    // abcdefgh with decimal point for c and d
    tm.displayText("abc.d.efgh");
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
}

static void Test6()
{
    // Test6  TEXT + ASCII combo
    // ADC=.2.948
    char text1[] = "ADC=.";
    tm.displayText(text1);
    tm.displayASCIIwDot(4, '2');
    tm.displayASCII(5, '9');
    tm.displayASCII(6, '4');
    tm.displayASCII(7, '8');
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.reset();
}

static void Test7()
{
    // TEST 7a Integer left aligned , NO leading zeros
    tm.displayIntNum(45, false, TMAlignTextLeft); // "45      "
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    // TEST 7b Integer left aligned , leading zeros
    tm.displayIntNum(99991, true, TMAlignTextLeft); // "00099991"
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.reset();
    // TEST 7c Integer right aligned , NO leading zeros
    tm.displayIntNum(35, false, TMAlignTextRight); // "      35"
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    // TEST 7d Integer right aligned , leading zeros
    tm.displayIntNum(9983551, true, TMAlignTextRight); // "09983551"
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));

    // TEST 7e tm.DisplayDecNumNIbble left aligned
    tm.DisplayDecNumNibble(134, 70, false, TMAlignTextLeft); // "134 " "70" , left aligned, NO leading zeros
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.DisplayDecNumNibble(23, 662, true, TMAlignTextLeft); // "0023" "0662" , left aligned , leading zeros
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.reset();

    // TEST 7f tm.DisplayDecNumNIbble right aligned
    tm.DisplayDecNumNibble(43, 991, false, TMAlignTextRight); // "  43" " 991" , right aligned, NO leading zeros
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.DisplayDecNumNibble(53, 8, true, TMAlignTextRight); // "0053" "0008" , right aligned , leading zeros
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
}

static void Test8()
{
    // TEST 8  TEXT STRING + integer SSSSIIII
    char workStr[11];
    uint16_t  data = 234;
    sprintf(workStr, "ADC=.%04d", data); // "ADC=.0234"
    tm.displayText(workStr);
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
}

static void Test9()
{
    // TEST 9 Text String + Float  SSSSFFFF ,  just one possible method.
    float voltage = 12.45;
    uint16_t temp = 0;
    char workStr[11];
    uint8_t  digit1, digit2, digit3 , digit4;
    voltage =  voltage * 100; // 1245
    temp = (uint16_t)voltage;
    digit1 = (temp / 1000) % 10;
    digit2 = (temp / 100) % 10;
    digit3 = (temp / 10) % 10;
    digit4 =  temp % 10;

    sprintf(workStr, "ADC=.%d%d.%d%d", digit1, digit2, digit3, digit4);
    tm.displayText(workStr); //12.45.VOLT
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.reset();
}

static void Test10()
{
    //TEST 10 Multiple dots test
    tm.displayText("Hello...");
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.displayText("...---..."); //SOS in morse
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
}

static void Test11()
{
    //TEST11 user overflow
    tm.displayText("1234567890abc"); //should display just 12345678
    vTaskDelay(pdMS_TO_TICKS(myTestDelay));
    tm.reset();
}

static void Test12()
{
    //TEST 12 scrolling text, just one possible method.
    char textScroll[17] = " Hello world 123";

    while(1) {
        tm.displayText(textScroll);
        vTaskDelay(pdMS_TO_TICKS(myTestDelay1));
        if (strlen(textScroll) > 0) {
            memmove(textScroll, textScroll + 1, strlen(textScroll));
            tm.displayText("        "); //Clear display or last character will drag across screen
        } else {
            return;
        }
    }

}


static void Test13()
{
    //Test 13 LED display
    uint8_t LEDposition = 0;

    // Test 13A Turn on redleds one by one, left to right, with setLED where 0 is L1 and 7 is L8 (L8 RHS of display)
    for (LEDposition = 0; LEDposition < 8; LEDposition++) {
        tm.setLED(LEDposition, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        tm.setLED(LEDposition, 0);
    }

    // TEST 13b test setLEDs function (0xLEDXX) ( L8-L1 , XX )
    // NOTE passed L8-L1 and on display L8 is on right hand side. i.e. 0x01 turns on L1. LXXX XXXX
    // For model 1 just use upper byte , lower byte is is used by model3 for bi-color leds leave at 0x00 for model 1.
    tm.setLEDs(0xFF00); //  all LEDs on
    vTaskDelay(pdMS_TO_TICKS(3000));
    tm.setLEDs(0x0100); // Displays as LXXX XXXX (L1-L8) , NOTE on display L8 is on right hand side.
    vTaskDelay(pdMS_TO_TICKS(3000));
    tm.setLEDs(0xF000); //  Displays as XXXX LLLL (L1-L8) , NOTE on display L8 is on right hand side.
    vTaskDelay(pdMS_TO_TICKS(3000));
    tm.setLEDs(0x0000); // all off
    vTaskDelay(pdMS_TO_TICKS(3000));

}



static void Test14()
{
    //Test 14 buttons and LED test, press switch number S-X to turn on LED-X, where x is 1-8.
    //The HEx value of switch is also sent to Serial port.
    tm.displayText("buttons ");
    while (1) { // Loop here forever
        uint8_t buttons = tm.readButtons();
        /* buttons contains a byte with values of button s8s7s6s5s4s3s2s1
         HEX  :  Switch no : Binary
         0x01 : S1 Pressed  0000 0001
         0x02 : S2 Pressed  0000 0010
         0x04 : S3 Pressed  0000 0100
         0x08 : S4 Pressed  0000 1000
         0x10 : S5 Pressed  0001 0000
         0x20 : S6 Pressed  0010 0000
         0x40 : S7 Pressed  0100 0000
         0x80 : S8 Pressed  1000 0000
        */
        doLEDs(buttons);
        tm.displayIntNum(buttons, false, TMAlignTextRight);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// scans the individual bits of value sets a LED based on which button pressed
static void doLEDs(uint8_t value)
{
    for (uint8_t LEDposition = 0; LEDposition < 8; LEDposition++) {
        tm.setLED(LEDposition, value & 1);
        value = value >> 1;
    }
}
