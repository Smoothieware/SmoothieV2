#include "StepTicker.h"

#include "AxisDefns.h"
#include "StepperMotor.h"
#include "Block.h"
#include "Conveyor.h"
#include "Module.h"
#include "tmr-setup.h"

#include <fcntl.h>
#include <errno.h>

#include <math.h>

#include "MemoryPool.h"

#ifdef STEPTICKER_DEBUG_PIN
// debug pins, only used if defined in src/makefile
#include "Pin.h"
Pin stepticker_debug_pin(STEPTICKER_DEBUG_PIN, Pin::AS_OUTPUT);
#define SET_STEPTICKER_DEBUG_PIN(n) { stepticker_debug_pin.set(n); }
#else
#define SET_STEPTICKER_DEBUG_PIN(n)
#endif

// TODO move ramfunc define to a utils.h
#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))
//#define _ramfunc_

StepTicker *StepTicker::instance = nullptr;
bool StepTicker::started = false;

StepTicker *StepTicker::getInstance()
{
    if(instance == nullptr) {
        instance = new(*_DTCMRAM) StepTicker;
    }

    return instance;
}
void StepTicker::deleteInstance()
{
    if(started) {
        instance->stop();
    }
    delete instance;
    instance = nullptr;
}

StepTicker::StepTicker()
{
    conveyor = Conveyor::getInstance();
}

StepTicker::~StepTicker()
{}

// ISR callbacks from timer
_ramfunc_ void StepTicker::step_timer_handler(void)
{
    StepTicker::getInstance()->step_tick();
}

// ISR callbacks from timer
_ramfunc_ void StepTicker::unstep_timer_handler(void)
{
    StepTicker::getInstance()->unstep_tick();
}

bool StepTicker::start()
{
    if(!started) {

        // setup the step tick timer, which handles step ticks and one off unstep interrupts
        int permod = steptimer_setup(frequency, delay, (void *)step_timer_handler, (void *)unstep_timer_handler);
        if(permod ==  0) {
            printf("ERROR: steptimer_setup failed\n");
            return false;
        }
        started = true;
    }

    current_tick = 0;

    return true;
}

bool StepTicker::stop()
{
    if(started) {
        steptimer_stop();
    }
    return true;
}

// Set the base stepping frequency
// can only be set before it is started
void StepTicker::set_frequency( float freq )
{
    if(started) {
        printf("ERROR: cannot set stepticker frequency after it has been started\n");
        return;
    }

    this->frequency = floorf(freq);
}

// Set the reset delay, must be called before started
void StepTicker::set_unstep_time( float microseconds )
{
    if(started) {
        printf("ERROR: cannot set stepticker unstep delay after it has been started\n");
        return;
    }

    // check that the unstep time is less than the step period
    uint32_t d = roundf(microseconds);
    uint32_t period = floorf(1000000.0F / frequency);
    if(d > period - 1) { // within 1us of the period
        printf("ERROR: cannot set stepticker unstep delay greater than or equal to step ticker period: %lu, %lu\n", delay, period);
        return;
    }

    delay = d;
}

_ramfunc_  bool StepTicker::start_unstep_ticker()
{
    unsteptimer_start();
    return true;
}

// Reset step pins on any motor that was stepped
_ramfunc_  void StepTicker::unstep_tick()
{
    uint32_t bitmsk = 1;
    for (int i = 0; i < num_motors; i++) {
        if(this->unstep & bitmsk) {
            this->motor[i]->unstep();
        }
        bitmsk <<= 1;
    }
    this->unstep = 0;
}

// step clock
_ramfunc_  void StepTicker::step_tick (void)
{
    //SET_STEPTICKER_DEBUG_PIN(running ? 1 : 0);

    if(unstep != 0) {
        // this is a failsafe, if we get here it means we missed the unstep from a previous tick
        // so we need to unstep the pin now or it will remain high
        unstep_tick();
        missed_unsteps++; // keep track for diagnostics
    }

    if(callback_fnc) {
        // call an external function
        int m = callback_fnc();
        if(m >= 0) {
            // we stepped so schedule an unstep
            unstep |= (1 << m);
        }
    }

    // if nothing has been setup we ignore the ticks
    if(!running) {
        // check if anything new available
        if(conveyor->get_next_block(&current_block)) { // returns false if no new block is available
            running = start_next_block(); // returns true if there is at least one motor with steps to issue
            if(!running) {
                if(unstep != 0) {
                    start_unstep_ticker();
                }
                return;
            }
        } else {
            if(unstep != 0) {
                start_unstep_ticker();
            }
            return;
        }
    }

    if(Module::is_halted()) {
        running = false;
        current_tick = 0;
        current_block = nullptr;
        return;
    }

    bool still_moving = false;
    // foreach motor, if it is active see if time to issue a step to that motor
    for (uint8_t m = 0; m < num_motors; m++) {
        auto *cur_motor = motor[m];
        auto& cur_tick_info = current_block->tick_info[m];

        // normal processing
        if(cur_tick_info.steps_to_move == 0) continue; // not active

        cur_tick_info.steps_per_tick += cur_tick_info.acceleration_change;

        if(current_tick == cur_tick_info.next_accel_event) {
            if(current_tick == current_block->accelerate_until) { // We are done accelerating, deceleration becomes 0 : plateau
                cur_tick_info.acceleration_change = 0;
                if(current_block->decelerate_after < current_block->total_move_ticks) {
                    cur_tick_info.next_accel_event = current_block->decelerate_after;
                    if(current_tick != current_block->decelerate_after) {
                        // We are plateauing
                        cur_tick_info.steps_per_tick = cur_tick_info.plateau_rate;
                    }
                }
            }

            if(current_tick == current_block->decelerate_after) { // We start decelerating
                cur_tick_info.acceleration_change = cur_tick_info.deceleration_change;
            }
        }

        // protect against rounding errors and such
        if(cur_tick_info.steps_per_tick <= 0) {
            cur_tick_info.counter = STEPTICKER_FPSCALE; // we force completion this step by setting to 1.0
            cur_tick_info.steps_per_tick = 0;
        }

        cur_tick_info.counter += cur_tick_info.steps_per_tick;

        if(cur_tick_info.counter >= STEPTICKER_FPSCALE) { // >= 1.0 step time
            cur_tick_info.counter -= STEPTICKER_FPSCALE; // -= 1.0;
            ++cur_tick_info.step_count;

            // step the motor
            bool ismoving = cur_motor->step(); // returns false if the moving flag was set to false externally (probes, endstops etc)
            // we stepped so schedule an unstep
            unstep |= (1 << m);

            if(!ismoving || cur_tick_info.step_count == cur_tick_info.steps_to_move) {
                // done
                cur_tick_info.steps_to_move = 0;
                cur_motor->stop_moving(); // let motor know it is no longer moving
            }
        }

        // see if any motors are still moving after this tick
        if(cur_motor->is_moving()) still_moving = true;
    }

    // We may have set a pin on in this tick, now we set the timer to set it off
    // right now it takes about 1-2us to get here which will add to the pulse width from when it was on
    // the pulse width will be 1us (or whatever it is set to) from this point on, so at least 2-3 us
    if(unstep != 0) {
        start_unstep_ticker();
    }

    // again we may only have forced steps to do
    if(!running) return;

    // do this after so we start at tick 0
    ++current_tick; // count number of ticks


    // see if any motors are still moving
    if(!still_moving) {
        //SET_STEPTICKER_DEBUG_PIN(0);

        // all moves finished
        current_tick = 0;

        // get next block
        // do it here so there is no delay in ticks
        conveyor->block_finished();

        if(conveyor->get_next_block(&current_block)) { // returns false if no new block is available
            running = start_next_block(); // returns true if there is at least one motor with steps to issue

        } else {
            current_block = nullptr;
            running = false;
        }

        // all moves finished
        // we delegate the slow stuff to the pendsv handler which will run as soon as this interrupt exits
        //NVIC_SetPendingIRQ(PendSV_IRQn); this doesn't work
        //SCB->ICSR = 0x10000000; // SCB_ICSR_PENDSVSET_Msk;
    }
}

// only called from the step tick ISR (single consumer)
_ramfunc_ bool StepTicker::start_next_block()
{
    if(current_block == nullptr) return false;

    bool ok = false;
    // need to prepare each active motor
    for (uint8_t m = 0; m < num_motors; m++) {
        if(current_block->tick_info[m].steps_to_move == 0) continue;

        ok = true; // mark at least one motor is moving
        // set direction bit here
        // NOTE this would be at least 10us before first step pulse.
        // TODO does this need to be done sooner, if so how without delaying next tick
        motor[m]->set_direction(current_block->direction_bits[m]);
        motor[m]->start_moving(); // also let motor know it is moving now
    }

    current_tick = 0;

    if(ok) {
        //SET_STEPTICKER_DEBUG_PIN(1);
        return true;

    } else {
        // this is an edge condition that should never happen, but we need to discard this block if it ever does
        // basically it is a block that has zero steps for all motors
        conveyor->block_finished();
    }

    return false;
}


// returns index of the stepper motor in the array and bitset
int StepTicker::register_actuator(StepperMotor* m)
{
    motor[num_motors++] = m;
    return num_motors - 1;
}

/*
Stats...
500mm/sec @ 100 steps/mm and 200KHz tick
ΔT  22.252314 ms
Nfalling    1.113 k
Nrising 1.113 k
fmin    49.793 kHz
fmax    50.209 kHz
fmean   49.999 kHz
Tstd    14.571 ns
Tavg    20 µs
Dcycle  7.977 %
Hmin    1.5 µs
Hmean   1.595 µs
HSDev   31.848 ns
Hmax    1.667 µs
Lmin    18.25 µs
Lmean   18.405 µs
LSDev   33.179 ns
Lmax    18.5 µs
*/
