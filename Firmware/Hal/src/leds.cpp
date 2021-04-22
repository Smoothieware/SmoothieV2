#include "Pin.h"


#ifdef BOARD_NUCLEO
static Pin leds[] = {
	Pin("PB0", Pin::AS_OUTPUT),
	Pin("PE.1", Pin::AS_OUTPUT),
	Pin("PB_14", Pin::AS_OUTPUT),
};
#define NLEDS 3
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


