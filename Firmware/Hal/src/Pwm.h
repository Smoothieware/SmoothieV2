#pragma once

#include <stdint.h>
#include <string>

class Pwm
{
public:
	Pwm(int number, int channel);
	virtual ~Pwm();
    std::string to_string() const { return pin_name; }
	bool is_valid() const { return valid; }
	// set duty cycle 0-1
	void set(float v);
	float get() const { return value; }
	uint32_t get_frequency() { return frequency; }
	bool setup(uint32_t freq);

private:
	uint32_t frequency;
	std::string pin_name;
	float value;
	bool valid{false};
};
