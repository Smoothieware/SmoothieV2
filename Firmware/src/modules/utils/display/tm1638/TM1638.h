#pragma once

#include <cstdint>

/*
* Ported from....
* Project Name: TM1638plus
* File: TM1638plus.h
* Description: TM1638plus.h header file STM32 library for TM1638 module(LED & KEY). Model 1 & Model 3
* Author: Gavin Lyons.
* Created April 2021
* URL: https://github.com/gavinlyonsrepo/STM32_projects
*/

#define TM_ACTIVATE 0x8F // Start up
#define TM_BUTTONS_MODE 0x42 // Buttons mode
#define TM_WRITE_LOC 0x44 // Write to a location
#define TM_WRITE_INC 0x40 // Incremental write
#define TM_SEG_ADR 0xC0  // leftmost segment Address C0 C2 C4 C6 C8 CA CC CE
#define TM_LEDS_ADR 0xC1  // Leftmost LED address C1 C3 C5 C7 C9 CB CD CF
#define TM_BRIGHT_ADR 0x88 // Brightness address
#define TM_BRIGHT_MASK 0x07 // Brightness mask
#define TM_DEFAULT_BRIGHTNESS 0x02 //can be 0x00 to 0x07
#define TM_DISPLAY_SIZE 8 //size of display

#define TM_ASCII_OFFSET 32 // Ascii table offset to jump over first missing 32 chars
#define TM_HEX_OFFSET   16 // Ascii table offset to reach number position
#define TM_DOT_MASK_DEC    128 // 0x80 Mask to  switch on decimal point in seven seg.

#define TM_OFF_LED 0x00
#define TM_RED_LED 0x02 // Model 3
#define TM_GREEN_LED 0x01 // Model 3

#define TM_READ_DELAY 5 // uS
#define TM_WRITE_DELAY 5 // uS

typedef enum {
    TMAlignTextRight = 1,
    TMAlignTextLeft = 2,
} AlignTextType_e; // Alignment of text

// font , map of ASCII values/table to 7-segment, offset to position 32.
const  unsigned char SevenSeg[] = {
    0x00, /* (space) */
    0x86, /* ! */
    0x22, /* " */
    0x7E, /* # */
    0x6D, /* $ */
    0xD2, /* % */
    0x46, /* & */
    0x20, /* ' */
    0x29, /* ( */
    0x0B, /* ) */
    0x21, /* * */
    0x70, /* + */
    0x10, /* , */
    0x40, /* - */
    0x80, /* . */
    0x52, /* / */
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F, /* 9 */
    0x09, /* : */
    0x0D, /* ; */
    0x61, /* < */
    0x48, /* = */
    0x43, /* > */
    0xD3, /* ? */
    0x5F, /* @ */
    0x77, /* A */
    0x7C, /* B */
    0x39, /* C */
    0x5E, /* D */
    0x79, /* E */
    0x71, /* F */
    0x3D, /* G */
    0x76, /* H */
    0x30, /* I */
    0x1E, /* J */
    0x75, /* K */
    0x38, /* L */
    0x15, /* M */
    0x37, /* N */
    0x3F, /* O */
    0x73, /* P */
    0x6B, /* Q */
    0x33, /* R */
    0x6D, /* S */
    0x78, /* T */
    0x3E, /* U */
    0x3E, /* V */
    0x2A, /* W */
    0x76, /* X */
    0x6E, /* Y */
    0x5B, /* Z */
    0x39, /* [ */
    0x64, /* \ */
    0x0F, /* ] */
    0x23, /* ^ */
    0x08, /* _ */
    0x02, /* ` */
    0x5F, /* a */
    0x7C, /* b */
    0x58, /* c */
    0x5E, /* d */
    0x7B, /* e */
    0x71, /* f */
    0x6F, /* g */
    0x74, /* h */
    0x10, /* i */
    0x0C, /* j */
    0x75, /* k */
    0x30, /* l */
    0x14, /* m */
    0x54, /* n */
    0x5C, /* o */
    0x73, /* p */
    0x67, /* q */
    0x50, /* r */
    0x6D, /* s */
    0x78, /* t */
    0x1C, /* u */
    0x1C, /* v */
    0x14, /* w */
    0x76, /* x */
    0x6E, /* y */
    0x5B, /* z */
};

#include "Module.h"
#include "Pin.h"

class ConfigReader;

class TM1638 : public Module
{

public:
    // Constructor
    TM1638();
    virtual ~TM1638();

    static bool create(ConfigReader& cr);
    bool configure(ConfigReader& cr);

    // Methods

    void displayBegin(void);     // Begin method , Call in setup

    void reset(void);  // Reset / Clear module

    //Sets the brightness level on a scale of brightness = 0 to 7.
    //0 is not turned off, it's just the lowest brightness.
    //If user wishes to change the default brightness at start-up change.
    //The DEFAULT_BRIGHTNESS define in header file.
    void brightness(uint8_t brightness);

    //Read buttons returns a byte with value of buttons 1-8 b7b6b5b4b3b2b1b0
    // 1 pressed, zero not pressed.
    uint8_t readButtons(void);

    // Send Text to Seven segments, passed char array pointer
    // dots are removed from string and dot on preceding digit switched on
    // "abc.def" will be shown as "abcdef" with c decimal point turned on.
    void displayText(const char *text);

    // Send ASCII value to seven segment, pass position 0-7 and ASCII value byte
    void displayASCII(uint8_t position, uint8_t ascii);

    // Same as displayASCII function but turns on dot/decimal point  as well
    void displayASCIIwDot(uint8_t position, uint8_t ascii) ;

    // Send HEX value to seven segment, pass position 0-7 and hex value(DEC) 0-15
    void displayHex(uint8_t position, uint8_t hex);

    // Send seven segment value to seven segment
    //  pass position 0-7 byte of data corresponding to segments (dp)gfedcba
    // i.e 0b01000001 will set g and a on.
    void display7Seg(uint8_t position, uint8_t value);

    //Display an integer and leading zeros optional
    void displayIntNum(unsigned long number, bool leadingZeros = true, AlignTextType_e = TMAlignTextLeft);

    //Divides the display into two nibbles and displays a Decimal number in each.
    //takes in two numbers 0-9999 for each nibble ,  and byte for decimal point display,
    //and leading zeros optional
    void DisplayDecNumNibble(uint16_t numberUpper, uint16_t numberLower, bool leadingZeros = true, AlignTextType_e = TMAlignTextLeft);

    // Set the LEDs. passed one  16bit integer.
    // MODEL 3:
    //MSB byte for the green LEDs, LS byte for the red LEDs (0xgreenred)
    //ie. 0xE007   1110 0000 0000 0111   results in L8-L0  GGGX XRRR, NOTE L8 is RHS on display
    // MODEL 1:
    // MSB byte 1 for  red LED , LSB byte n/a set to 0x00 (0xleds, 0xXX)
    //i.e 0xF100  1111 0000 L8-L0 RRRRXXX0 NOTE  L8 is RHS on display
    void setLEDs(uint16_t greenred);

    // Set an LED, pass it LED position 0-7 and value 0 or 1 , L1-L8
    void setLED(uint8_t position, uint8_t value);

    // Mutex to stop concurrent access, the caller is responsible for locking and unlocking access
    bool lock();
    void unlock();

private:
    void sendCommand(uint8_t value);
    void sendData(uint8_t  data);
    uint8_t HighFreqshiftin();
    void HighFreqshiftOut(uint8_t val);
    void button_tick();

    void *plock{nullptr};
    Pin clock_pin;
    Pin data_pin;
    Pin strobe_pin;
};
