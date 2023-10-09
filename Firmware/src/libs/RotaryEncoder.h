#pragma once

#include <cstdint>
#include <functional>

class Pin;

class RotaryEncoder
{
public:
	RotaryEncoder(Pin& p1, Pin& p2);
    RotaryEncoder(Pin& p1, Pin& p2, std::function<void(void)> cb);
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
    std::function<void(void)> callback{nullptr};
};
