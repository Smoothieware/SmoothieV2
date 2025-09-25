#include "StepperMotor.h"
#include "StepTicker.h"
#include "OutputStream.h"

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
    step_count= 0;
    step_count_homed= 0;
    moving= false;
    acceleration= -1;
    selected= true;
    extruder= false;
    p_slave= nullptr;

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
    step_count_homed = step_count - last_milestone_steps;
}

void StepperMotor::change_last_milestone(float new_milestone)
{
    last_milestone_mm = new_milestone;
    last_milestone_steps = roundf(last_milestone_mm * steps_per_mm);
    step_count_homed = step_count - last_milestone_steps;
}

// Used by extruder saving state
void StepperMotor::set_last_milestones(float mm, int32_t steps)
{
    last_milestone_mm= mm;
    last_milestone_steps= steps;
    step_count_homed = step_count - last_milestone_steps;
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
void StepperMotor::manual_step(bool dir)
{
    // set direction if needed
    if(this->direction != dir) {
        this->direction= dir;
        this->dir_pin.set(dir);
    }

    // this will be picked up by the stepticker and issue a step pulse
    ++forced_steps;
    StepTicker::getInstance()->set_check_forced_steps();
}


#ifdef DRIVER_TMC
// prime has TMC2590 or TMC2660 drivers so this handles the setup of those drivers
#include "TMC2590.h"
#include "TMC26X.h"

extern int find_actuator_key(const char *k);
bool StepperMotor::vmot= false;
bool StepperMotor::setup_tmc(ConfigReader& cr, const char *actuator_name, uint32_t type)
{
    // NOTE axis is only used by the TMC driver to identify itself in the designator field of M911
    char axis= motor_id<3 ? 'X'+motor_id : 'A'+motor_id-3;
    printf("DEBUG: setup_tmc: setting up tmc%lu for %s, axis %c (%d)\n", type, actuator_name, axis, motor_id);
    if(type == 2590) {
        tmc= new TMC2590(axis);
    }else if(type == 2660){
        tmc= new TMC26X(axis);
    }else{
        printf("ERROR: setup_tmc: tmc%lu is not a valid driver\n", type);
        return false;
    }

    if(!tmc->config(cr, actuator_name)) {
        delete tmc;
        return false;
    }
    tmc->init();
    tmc_type = type;

    return true;
}

bool StepperMotor::set_current(float c)
{
    // we need to do this here to both make the current the same and because the current module won't see the slave
    if(p_slave != nullptr) p_slave->set_current(c);

    if(tmc == nullptr) return false;
    // send current to TMC
    current_ma= roundf(c*1000.0F);
    tmc->setCurrent(current_ma); // sets current in milliamps
    return true;
}

bool StepperMotor::set_microsteps(uint16_t ms)
{
    if(tmc == nullptr) return false;
    tmc->setMicrosteps(ms); // sets microsteps
    return true;
}

int StepperMotor::get_microsteps()
{
    if(tmc == nullptr) return 0;
    return tmc->getMicrosteps();
}

void StepperMotor::enable(bool state)
{
    // we need to do this here becuase the higher level won't see the slave
    if(p_slave != nullptr) p_slave->enable(state);

    if(tmc == nullptr) {
        if(en_pin.connected()) {
            // set to true to enable the chip (en may need to be inverted)
            en_pin.set(state);
        }
        return;
    }

    if(state && !vmot){
        //printf("WARNING: %d: trying to enable motors when vmotor is off\n", motor_id);
        if(is_enabled()) {
            tmc->setEnabled(false);
        }
        return;
    }

    // if we have lost Vmotor since last time then we need to re load all the drivers configs
    if(state && vmot_lost) {
        if(vmot) {
            tmc->init();
            tmc->setEnabled(true);
            vmot_lost= false;
            printf("DEBUG: tmc: %d inited\n", motor_id);
        }else{
            tmc->setEnabled(false);
        }
        return;
    }

    // we need to make sure that this is atomic and protected against another thread setting the current
    if(state) tmc->lock(true);
    tmc->setEnabled(state);

    if(state) {
        // make sure current is restored before the move if we were at standstill
        if(tmc->get_status() & TMCBase::IS_STANDSTILL_CURRENT) {
            tmc->setCurrent(current_ma); // resets current in milliamps
            printf("DEBUG: %d standstill current reset to %lu mA\n", motor_id, current_ma);
        }
    }
    if(state) tmc->lock(false);
}

bool StepperMotor::is_enabled() const
{
    if(tmc == nullptr) {
        if(en_pin.connected()) {
            return en_pin.get();
        }
        // presume always enabled
        return true;
    }
    return tmc->isEnabled();
}

void StepperMotor::dump_status(OutputStream& os, bool flag)
{
    if(tmc == nullptr) return;
    tmc->dump_status(os, flag);
    if(p_slave != nullptr) {
        os.printf("----- Slave actuator -----\n");
        p_slave->dump_status(os, flag);
    }
}

void StepperMotor::set_raw_register(OutputStream& os, uint32_t reg, uint32_t val)
{
    if(tmc == nullptr) return;
    tmc->set_raw_register(os, reg, val);
    // we need to do this here because the higher level won't see the slave
    if(p_slave != nullptr) p_slave->set_raw_register(os, reg, val);
}

bool StepperMotor::set_options(GCode& gcode)
{
    if(tmc == nullptr) return false;
    if(tmc->set_options(gcode)) {
        // we need to do this here because the higher level won't see the slave
        if(p_slave != nullptr && !p_slave->set_options(gcode)) return false;

    }else{
        return false;
    }
    return true;
}

bool StepperMotor::check_driver_error()
{
    if(tmc == nullptr) return false;
    if(!tmc->check_errors()) return false;
    if(p_slave != nullptr) {
        if(!p_slave->check_driver_error()) return false;
    }

    return true;
}

bool StepperMotor::init_slave(StepperMotor *sm)
{
    // do some sanity checks that the slave and master have the same settings
    if(sm->tmc_type != tmc_type) {
        printf("ERROR: stepper-motor init-slave: slave %d does not have the same driver type as master %d\n", sm->motor_id, motor_id);
        return false;
    }

    if(sm->get_microsteps() != get_microsteps()) {
        printf("ERROR: stepper-motor init-slave: slave %d does not have the same microstepping as master %d\n", sm->motor_id, motor_id);
        return false;
    }

    p_slave = sm;
    return true;
}

#else

// Mini has enable pins on the drivers

void StepperMotor::enable(bool state)
{
    en_pin.set(state);
}

bool StepperMotor::is_enabled() const
{
    return en_pin.get();
}

#endif
