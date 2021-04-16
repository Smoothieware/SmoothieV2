#include "Pwm.h"

#include <cstring>
#include <cctype>
#include <tuple>
#include <vector>
#include <cmath>

// allocated timers and channels
// we have two timers and each has 4 channels
// each timer can have a different frequency
// channels on the same timer have the same frequency but different duty cycles
bool Pwm::allocated[2][4];

Pwm::Pwm(int n, int ch)
{
	valid= false;
	index= 0;
}

// static
bool Pwm::setup(uint32_t freq)
{
    //HAL_TIM_PWM_ConfigChannel
    frequency= freq;
	return true;
}

void Pwm::set(float v)
{
	if(!valid) return;

	if(v < 0) v= 0;
	else if(v > 1) v= 1;

	value= v;
}
