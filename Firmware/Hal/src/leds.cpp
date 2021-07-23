#include "Pin.h"

#if defined(BOARD_PRIME)
    Pin("PH9", Pin::AS_OUTPUT),
    Pin("PH10", Pin::AS_OUTPUT),
    Pin("PH11", Pin::AS_OUTPUT),
    Pin("PH12", Pin::AS_OUTPUT),
#define NLEDS 4

#elif defined(BOARD_NUCLEO)
static Pin leds[] = {
	Pin("PB0", Pin::AS_OUTPUT),
	Pin("PE.1", Pin::AS_OUTPUT),
	//Pin("PB_14", Pin::AS_OUTPUT),
};
#define NLEDS 2

#elif defined(BOARD_DEVEBOX)
static Pin leds[] = {
	Pin("PA1", Pin::AS_OUTPUT),
};
#define NLEDS 1

#else
	#error unrecognized board
#endif

extern "C" void Board_LED_Toggle(uint8_t led)
{
	if(led >= NLEDS) return;
	leds[led].toggle();
}

extern "C" void Board_LED_Set(uint8_t led, bool on)
{
	if(led >= NLEDS) return;
	leds[led].set(on);
}

extern "C" bool Board_LED_Test(uint8_t led)
{
	if(led >= NLEDS) return false;
	return leds[led].get();
}


