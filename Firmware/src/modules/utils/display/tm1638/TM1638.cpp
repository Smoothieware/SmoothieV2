// Model 1 type TM1638 7 segment LED display
// Ported from https://github.com/gavinlyonsrepo/TM1638plus

#include "TM1638.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "main.h"

#include <cstring>

#define enable_key "enable"
#define clock_pin_key "clock_pin"
#define data_pin_key "data_pin"
#define strobe_pin_key "strobe_pin"

REGISTER_MODULE(TM1638, TM1638::create)

bool TM1638::create(ConfigReader& cr)
{
    printf("DEBUG: configure TM1638 button\n");
    TM1638 *tm = new TM1638();
    if(!tm->configure(cr)) {
        printf("INFO: TM1638 not enabled\n");
        delete tm;
    }
    return true;
}

TM1638::TM1638() : Module("tm1638")
{
}

bool TM1638::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("tm1638", m)) return false;

    if(!cr.get_bool(m, enable_key, false)) {
        return false;
    }

    if(clock_pin.from_string(cr.get_string(m,  clock_pin_key , "nc"))) {
        if(clock_pin.as_output()) clock_pin.set(false);
    }
    if(data_pin.from_string(cr.get_string(m,  data_pin_key , "nc"))) {
        if(data_pin.as_output()) data_pin.set(false);
    }
    if(strobe_pin.from_string(cr.get_string(m,  strobe_pin_key , "nc"))) {
        if(strobe_pin.as_output()) strobe_pin.set(false);
    }

    if(!clock_pin.connected() || !data_pin.connected() || !strobe_pin.connected()) {
        printf("ERROR: TM1638-configure - pin assignment error\n");
        return false;
    }

    // SlowTicker::getInstance()->attach(5, std::bind(&TM1638::button_tick, this));

    return true;
}

void TM1638::button_tick()
{
    // could read buttons here
}

void TM1638::displayBegin()
{
    sendCommand(TM_ACTIVATE);
    brightness(TM_DEFAULT_BRIGHTNESS);
    reset();
}

void TM1638::sendCommand(uint8_t value)
{
    strobe_pin.set(false);
    sendData(value);
    strobe_pin.set(true);
}

void TM1638::sendData(uint8_t data)
{
    HighFreqshiftOut(data);
}

void TM1638::reset()
{
    sendCommand(TM_WRITE_INC); // set auto increment mode
    strobe_pin.set(false);
    sendData(TM_SEG_ADR);  // set starting address to
    for (uint8_t i = 0; i < 16; i++) {
        sendData(0x00);
    }
    strobe_pin.set(true);
}

void TM1638::setLED(uint8_t position, uint8_t value)
{
    data_pin.as_output();
    sendCommand(TM_WRITE_LOC);
    strobe_pin.set(false);
    sendData(TM_LEDS_ADR + (position << 1));
    sendData(value);
    strobe_pin.set(true);
}

void TM1638::setLEDs(uint16_t ledvalues)
{
    for (uint8_t LEDposition = 0;  LEDposition < 8; LEDposition++) {
        uint8_t colour = 0;

        if ((ledvalues & (1 << LEDposition)) != 0) {
            colour |= TM_RED_LED; //scan lower byte, set Red if one
        }

        if ((ledvalues & (1 << (LEDposition + 8))) != 0) {
            colour |= TM_GREEN_LED; //scan upper byte, set green if one
        }

        setLED(LEDposition, colour);
    }
}

void TM1638::displayIntNum(unsigned long number, bool leadingZeros, AlignTextType_e TextAlignment)
{
    char values[TM_DISPLAY_SIZE + 1];
    char TextDisplay[5] = "%";
    char TextLeft[3] = "ld";
    char TextRight[4] = "8ld";

    if (TextAlignment == TMAlignTextLeft) {
        strcat(TextDisplay , TextLeft); // %ld
    } else if ( TextAlignment == TMAlignTextRight) {
        strcat(TextDisplay , TextRight); // %8ld
    }

    snprintf(values, TM_DISPLAY_SIZE + 1, leadingZeros ? "%08ld" : TextDisplay, number);
    displayText(values);
}

void TM1638::DisplayDecNumNibble(uint16_t  numberUpper, uint16_t numberLower, bool leadingZeros, AlignTextType_e TextAlignment)
{
    char valuesUpper[TM_DISPLAY_SIZE + 1];
    char valuesLower[TM_DISPLAY_SIZE / 2 + 1];
    char TextDisplay[5] = "%";
    char TextLeft[4] = "-4d";
    char TextRight[3] = "4d";

    if (TextAlignment == TMAlignTextLeft) {
        strcat(TextDisplay , TextLeft); // %-4d
    } else if ( TextAlignment == TMAlignTextRight) {
        strcat(TextDisplay , TextRight); // %4d
    }

    snprintf(valuesUpper, TM_DISPLAY_SIZE / 2 + 1, leadingZeros ? "%04d" : TextDisplay, numberUpper);
    snprintf(valuesLower, TM_DISPLAY_SIZE / 2 + 1, leadingZeros ? "%04d" : TextDisplay, numberLower);

    strcat(valuesUpper , valuesLower);
    displayText(valuesUpper);
}

void TM1638::displayText(const char *text)
{
    char c, pos;
    pos = 0;
    while ((c = (*text++)) && pos < TM_DISPLAY_SIZE)  {
        if (*text == '.' && c != '.') {
            displayASCIIwDot(pos++, c);

            text++;
        }  else {
            displayASCII(pos++, c);
        }
    }
}

void TM1638::displayASCIIwDot(uint8_t position, uint8_t ascii)
{
    // add 128 or 0x080 0b1000000 to turn on decimal point/dot in seven seg
    display7Seg(position, SevenSeg[ascii - TM_ASCII_OFFSET] + TM_DOT_MASK_DEC);
}

void TM1638::display7Seg(uint8_t position, uint8_t value)   // call 7-segment
{
    sendCommand(TM_WRITE_LOC);
    strobe_pin.set(false);
    sendData(TM_SEG_ADR + (position << 1));
    sendData(value);
    strobe_pin.set(true);
}

void TM1638::displayASCII(uint8_t position, uint8_t ascii)
{
    display7Seg(position, SevenSeg[ascii - TM_ASCII_OFFSET]);
}

void TM1638::displayHex(uint8_t position, uint8_t hex)
{
    uint8_t offset = 0;
    hex = hex % 16;
    if (hex <= 9) {
        display7Seg(position, SevenSeg[hex + TM_HEX_OFFSET]);
        // 16 is offset in reduced ASCII table for 0
    } else if ((hex >= 10) && (hex <= 15)) {
        // Calculate offset in reduced ASCII table for AbCDeF
        switch(hex) {
            case 10: offset = 'A'; break;
            case 11: offset = 'b'; break;
            case 12: offset = 'C'; break;
            case 13: offset = 'd'; break;
            case 14: offset = 'E'; break;
            case 15: offset = 'F'; break;
        }
        display7Seg(position, SevenSeg[offset - TM_ASCII_OFFSET]);
    }

}

uint8_t TM1638::readButtons()
{
    uint8_t buttons = 0;
    uint8_t v = 0;
    strobe_pin.set(false);
    sendData(TM_BUTTONS_MODE);
    data_pin.as_input();

    for (uint8_t i = 0; i < 4; i++) {
        v = HighFreqshiftin() << i;
        buttons |= v;
    }
    data_pin.as_output();

    return buttons;
}

void TM1638::brightness(uint8_t brightness)
{
    uint8_t  value = 0;
    value = TM_BRIGHT_ADR + (TM_BRIGHT_MASK & brightness);
    sendCommand(value);
}

#include "benchmark_timer.h"

static void delayMicroseconds(uint32_t us)
{
    uint32_t st= benchmark_timer_start();
    while(benchmark_timer_as_us(benchmark_timer_elapsed(st)) < us) ;
}

uint8_t  TM1638::HighFreqshiftin()
{
    uint8_t value = 0;
    uint8_t i = 0;

    for(i = 0; i < 8; ++i) {
        clock_pin.set(true);
        delayMicroseconds(TM_READ_DELAY);
        value |= ((data_pin.get() ? 1 : 0) << i);
        clock_pin.set(false);
        delayMicroseconds(TM_READ_DELAY);
    }
    return value;
}

void TM1638::HighFreqshiftOut(uint8_t val)
{
    uint8_t i;

    for (i = 0; i < 8; i++)  {
        if (!!(val & (1 << i))) {
            data_pin.set(true);
        } else {
            data_pin.set(false);
        }
        clock_pin.set(true);
        delayMicroseconds(TM_READ_DELAY);
        clock_pin.set(false);
        delayMicroseconds(TM_READ_DELAY);
    }
}
