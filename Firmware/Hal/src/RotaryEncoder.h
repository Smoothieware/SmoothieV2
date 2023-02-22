#pragma once

#include <cstdint>

class Pin;

class RotaryEncoder
{
public:
	RotaryEncoder(Pin& p1, Pin& p2);
	bool setup();
	uint32_t get_count() const { return count; }
	void reset_count() { count= 0; }

private:
	uint8_t process();
	void handle_en_irq();

	uint32_t count;
	uint8_t state;
	Pin& pin1;
	Pin& pin2;
};
