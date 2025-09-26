#pragma once

#include <stdint.h>
#include <array>
#include <bitset>
#include <functional>

#include "ActuatorCoordinates.h"

class StepperMotor;
class Block;
class Conveyor;

// handle 2.62 Fixed point
#define STEP_TICKER_FREQUENCY (StepTicker::getInstance()->get_frequency())
#define STEPTICKER_FPSCALE (1LL<<62)
#define STEPTICKER_FROMFP(x) ((float)(x)/STEPTICKER_FPSCALE)

class StepTicker
{
public:
    static StepTicker *getInstance();
    static void deleteInstance();

    // delete copy and move constructors and assign operators
    StepTicker(StepTicker const&) = delete;             // Copy construct
    StepTicker(StepTicker&&) = delete;                  // Move construct
    StepTicker& operator=(StepTicker const&) = delete;  // Copy assign
    StepTicker& operator=(StepTicker &&) = delete;      // Move assign

    void set_frequency( float frequency );
    void set_unstep_time( float microseconds );
    int register_actuator(StepperMotor* motor);
    float get_frequency() const { return frequency; }
    const Block *get_current_block() const { return current_block; }
    bool start();
    bool stop();

    // can be set by a module to get called at stepticker frequency (currently only used by Lathe module)
    // return the motor number that needs to be unstepped if a step was made, or -1
    std::function<int()> callback_fnc{nullptr};

private:
    static StepTicker *instance;
    StepTicker();
    ~StepTicker();

    void unstep_tick();
    void step_tick (void);
    bool start_unstep_ticker();
    int initial_setup(const char *dev, void *timer_handler, uint32_t per);
    bool start_next_block();

    static void step_timer_handler(void);
    static void unstep_timer_handler(void);

    StepperMotor* motor[k_max_actuators];

    uint32_t unstep{0}; // one bit set per motor to indicate step pin needs to be unstepped
    uint32_t missed_unsteps{0};

    Block *current_block{nullptr};
    Conveyor *conveyor;

    uint32_t frequency{100000}; // 100KHz
    uint32_t delay{1}; //microseconds

    uint32_t current_tick{0};

    uint8_t num_motors{0};

    volatile bool running{false};
    static bool started;
};
