#include "StepperMotor.h"
#include "StepTicker.h"

#include <math.h>

StepperMotor::StepperMotor(Pin &step, Pin &dir, Pin &en) : step_pin(step), dir_pin(dir), en_pin(en)
{
    if(en.connected()) {
        //set_high_on_debug(en.port_number, en.pin);
    }

    steps_per_mm         = 1.0F;
    max_rate             = 50.0F;

    last_milestone_steps = 0;
    last_milestone_mm    = 0.0F;
    current_position_steps= 0;
    moving= false;
    acceleration= NAN;
    selected= true;
    extruder= false;

    enable(false);
    unstep(); // initialize step pin
    set_direction(false); // initialize dir pin
}

StepperMotor::~StepperMotor()
{
}

void StepperMotor::change_steps_per_mm(float new_steps)
{
    steps_per_mm = new_steps;
    last_milestone_steps = roundf(last_milestone_mm * steps_per_mm);
    current_position_steps = last_milestone_steps;
}

void StepperMotor::change_last_milestone(float new_milestone)
{
    last_milestone_mm = new_milestone;
    last_milestone_steps = roundf(last_milestone_mm * steps_per_mm);
    current_position_steps = last_milestone_steps;
}

void StepperMotor::set_last_milestones(float mm, int32_t steps)
{
    last_milestone_mm= mm;
    last_milestone_steps= steps;
    current_position_steps= last_milestone_steps;
}

void StepperMotor::update_last_milestones(float mm, int32_t steps)
{
    last_milestone_steps += steps;
    last_milestone_mm = mm;
}

int32_t StepperMotor::steps_to_target(float target)
{
    int32_t target_steps = roundf(target * steps_per_mm);
    return target_steps - last_milestone_steps;
}

// Does a manual step pulse, used for direct encoder control of a stepper
// NOTE manual step is experimental and may change and/or be removed in the future, it is an unsupported feature.
// use at your own risk
#include "benchmark_timer.h"
void StepperMotor::manual_step(bool dir)
{
    if(!is_enabled()) enable(true);

    // set direction if needed
    if(this->direction != dir) {
        this->direction= dir;
        this->dir_pin.set(dir);
        uint32_t st= benchmark_timer_start();
        while(benchmark_timer_as_us(benchmark_timer_elapsed(st)) < 1) ;
    }

    // pulse step pin
    this->step_pin.set(1);
    uint32_t st= benchmark_timer_start();
    while(benchmark_timer_as_us(benchmark_timer_elapsed(st)) < 3) ;
    this->step_pin.set(0);

    // keep track of actuators actual position in steps
    this->current_position_steps += (dir ? -1 : 1);
}


#ifdef DRIVER_TMC2590
// prime has TMC2590 drivers so this handles the setup of those drivers
#include "TMC2590.h"

bool StepperMotor::vmot= false;
bool StepperMotor::setup_tmc2590(ConfigReader& cr, const char *actuator_name)
{
    char axis= motor_id<3?'X'+motor_id:'A'+motor_id-3;
    printf("DEBUG: setting up tmc2590 for %s, axis %c\n", actuator_name, axis);
    tmc2590= new TMC2590(axis);
    if(!tmc2590->config(cr, actuator_name)) {
        delete tmc2590;
        return false;
    }
    tmc2590->init();

    return true;
}

bool StepperMotor::init_tmc2590()
{
    if(tmc2590 == nullptr) return false;
    tmc2590->init();
    return true;
}

bool StepperMotor::set_current(float c)
{
    if(tmc2590 == nullptr) return false;
    // send current to TMC2590
    tmc2590->setCurrent(c*1000.0F); // sets current in milliamps
    return true;
}

bool StepperMotor::set_microsteps(uint16_t ms)
{
    if(tmc2590 == nullptr) return false;
    tmc2590->setMicrosteps(ms); // sets microsteps
    return true;
}

int StepperMotor::get_microsteps()
{
    if(tmc2590 == nullptr) return 0;
    return tmc2590->getMicrosteps();
}

void StepperMotor::enable(bool state)
{
    if(tmc2590 == nullptr) {
        if(en_pin.connected()) {
            en_pin.set(!state);
        }
        return;
    }

    if(state && !vmot){
        //printf("WARNING: %d: trying to enable motors when vmotor is off\n", motor_id);
        if(is_enabled())
            tmc2590->setEnabled(false);
        return;
    }

    // if we have lost Vmotor since last time then we need to re load all the drivers configs
    if(state && vmot_lost) {
        if(vmot) {
            tmc2590->init();
            tmc2590->setEnabled(true);
            vmot_lost= false;
            printf("DEBUG: tmc2590: %d inited\n", motor_id);
        }else{
            tmc2590->setEnabled(false);
        }
        return;
    }

    // we don't want to enable/disable it if it is already in that state to avoid sending SPI all the time
    bool en= is_enabled();
    if((!en && state) || (en && !state)) {
        tmc2590->setEnabled(state);
    }
}

bool StepperMotor::is_enabled() const
{
    if(tmc2590 == nullptr) {
        if(en_pin.connected()) {
            return !en_pin.get();
        }
        // presume always enabled
        return true;
    }
    return tmc2590->isEnabled();
}

void StepperMotor::dump_status(OutputStream& os, bool flag)
{
    if(tmc2590 == nullptr) return;
    tmc2590->dump_status(os, flag);
}

void StepperMotor::set_raw_register(OutputStream& os, uint32_t reg, uint32_t val)
{
    if(tmc2590 == nullptr) return;
    tmc2590->set_raw_register(os, reg, val);
}

bool StepperMotor::set_options(GCode& gcode)
{
    if(tmc2590 == nullptr) return false;
    return tmc2590->set_options(gcode);
}

bool StepperMotor::check_driver_error()
{
    if(tmc2590 == nullptr) return false;
    return tmc2590->check_errors();
}

#else

// Mini has enable pins on the drivers

void StepperMotor::enable(bool state)
{
    en_pin.set(!state);
}

bool StepperMotor::is_enabled() const
{
    return !en_pin.get();
}

#endif
