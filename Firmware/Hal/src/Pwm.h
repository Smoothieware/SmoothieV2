#pragma once

#include <stdint.h>
#include <string>

class Pwm
{
public:
	Pwm(const char *name);
	virtual ~Pwm();
    std::string to_string() const { return name; }
	bool is_valid() const { return valid; }
	// set duty cycle 0-1
	void set(float v);
    float set_microseconds(float v);
	float get() const { return value; }
	uint32_t get_frequency() const { return instances[timr].frequency; }
    void set_frequency(uint32_t freq);

	static bool post_config_setup();
	static bool setup(int timr, uint32_t frequency);
	static bool is_allocated(int i, int j) { return allocated[i][j]; }
	using instance_t = struct { void *_htim; uint32_t period; uint32_t frequency; };

private:
	static instance_t instances[2];
	static bool allocated[2][4];
	uint8_t channel, timr;
	std::string name;
	float value{0};
	bool valid{false};
};
