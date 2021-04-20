#include "Pin.h"


#ifdef BOARD_NUCLEO
Pin leds[] = {
	Pin("PB0", Pin::AS_OUTPUT),
	Pin("PE.1", Pin::AS_OUTPUT),
	Pin("PB_14", Pin::AS_OUTPUT),
};
#else
	#error unrecognized board
#endif

extern "C" void Board_LED_Toggle(int led)
{
	leds[led].toggle();
}

extern "C" void Board_LED_Set(int led, bool on)
{
	leds[led].set(on);
}

extern "C" bool Board_LED_Test(int led)
{
	return leds[led].get();
}


