#include "RotaryEncoder.h"
#include "Pin.h"

// Enable this to emit codes twice per step.
//#define HALF_STEP

#define R_START 0x0
// Values returned by 'process'
// No complete step yet.
#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Anti-clockwise step.
#define DIR_CCW 0x20

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
const unsigned char ttable[6][4] = {
    // R_START (00)
    {R_START_M,            R_CW_BEGIN,     R_CCW_BEGIN,  R_START},
    // R_CCW_BEGIN
    {R_START_M | DIR_CCW, R_START,        R_CCW_BEGIN,  R_START},
    // R_CW_BEGIN
    {R_START_M | DIR_CW,  R_CW_BEGIN,     R_START,      R_START},
    // R_START_M (11)
    {R_START_M,            R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},
    // R_CW_BEGIN_M
    {R_START_M,            R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},
    // R_CCW_BEGIN_M
    {R_START_M,            R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW},
};

#else

// Use the full-step state table (emits a code at 00 only)
#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6


const uint8_t ttable[7][4] = {
    // R_START
    {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
    // R_CW_FINAL
    {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
    // R_CW_BEGIN
    {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
    // R_CW_NEXT
    {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
    // R_CCW_BEGIN
    {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
    // R_CCW_FINAL
    {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
    // R_CCW_NEXT
    {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};
#endif

RotaryEncoder::RotaryEncoder(Pin& p1, Pin& p2) : pin1(p1), pin2(p2)
{
    state= R_START;
    count= 0;
}

uint8_t RotaryEncoder::process()
{
    // Grab state of input pins.
    uint8_t pinstate = (pin2.get() ? 2 : 0) | (pin1.get() ? 1 : 0);
    // Determine new state from the pins and state table.
    state = ttable[state & 0xf][pinstate];
    // Return emit bits, ie the generated event.
    return state & 0x30;
}

void RotaryEncoder::handle_en_irq()
{
    uint8_t result = process();
    if (result == DIR_CW) {
        ++count;
    } else if (result == DIR_CCW) {
        --count;
    }
}

bool RotaryEncoder::setup()
{
    pin1.as_interrupt(std::bind(&RotaryEncoder::handle_en_irq, this), Pin::CHANGE);
    if(!pin1.connected()) {
        printf("ERROR: RotaryEncoder: Cannot set interrupt pin %s\n", pin1.to_string().c_str());
        return false;
    }
    pin2.as_interrupt(std::bind(&RotaryEncoder::handle_en_irq, this), Pin::CHANGE);
    if(!pin2.connected()) {
        printf("ERROR: RotaryEncoder: Cannot set interrupt pin %s\n", pin2.to_string().c_str());
        return false;
    }

    return true;
}

/*
// Example of use
bool use_rotary_encoder()
{
    // pin1 and pin2 must be interrupt capable pins that have not already got interrupts assigned for that line number
    Pin pin1("PJ8^"), pin2("PJ10^");
    RotaryEncoder enc(pin1, pin2);
    if(!enc.setup()) {
        printf("Failed to setup rotary encoder\n");
    }

    while(1) {
        uint32_t c;
        c= enc.get_count();
        printf("Encoder count= %lu\n", c);
        //safe_sleep(200);
    }

}
*/
