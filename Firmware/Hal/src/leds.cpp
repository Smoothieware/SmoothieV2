#include "Pin.h"

#include <vector>
#if defined(BOARD_PRIME)
static const char *led_pins[] = {
    "PH9",
    "PH10",
    "PH11",
    "PH12",
    nullptr
};

#elif defined(BOARD_NUCLEO)
static const char *led_pins[] = {
    "PB0",
    "PE1",
    "PB14",
    nullptr
};

#elif defined(BOARD_DEVEBOX)
static const char *led_pins[] = {
    "PA1",
    "PE11",
    "PE12",
    "PE13",
    nullptr
};

#else
static const char *led_pins[] = {nullptr};
#endif

static std::vector<Pin*> leds;

bool Board_LED_Init()
{
    const char **pp= led_pins;
    const char *p;
    while((p=*pp++) != nullptr) {
        leds.push_back(new Pin(p, Pin::AS_OUTPUT));
    }

    return true;
}

void Board_LED_Toggle(uint8_t led)
{
    if(led >= leds.size()) return;
    leds[led]->toggle();
}

void Board_LED_Set(uint8_t led, bool on)
{
    if(led >= leds.size()) return;
    leds[led]->set(on);
}

bool Board_LED_Test(uint8_t led)
{
    if(led >= leds.size()) return false;
    return leds[led]->get();
}

bool Board_LED_Assign(uint8_t led, Pin *pin)
{
    if(led >= leds.size()) return false;
    // leds[led]= pin;
    return false;
}
