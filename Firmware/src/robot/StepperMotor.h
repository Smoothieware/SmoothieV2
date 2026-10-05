#pragma once

#include "Pin.h"

class TMCBase;
class ConfigReader;
class OutputStream;
class GCode;

class StepperMotor
{
    public:
        StepperMotor(Pin& step, Pin& dir, Pin& en);
        ~StepperMotor();

        void set_motor_id(uint8_t id) { motor_id= id; }
        uint8_t get_motor_id() const { return motor_id; }

        // called from step ticker ISR
        inline bool step() {
            step_pin.set(1); step_count += (direction?-1:1);
            if(p_slave != nullptr) p_slave->step();
            return moving;
        }
        // called from unstep ISR
        inline void unstep() {
            step_pin.set(0);
            if(p_slave != nullptr) p_slave->unstep();
        }

        // called from step ticker ISR
        inline void set_direction(bool f) {
            dir_pin.set(f); direction= f;
            if(p_slave != nullptr) p_slave->set_direction(f);
        }
        inline bool get_direction() const { return direction; }
        // called from step ticker ISR
        inline void inc_steps(int32_t s) {
            step_count += s;
            if(p_slave != nullptr) { p_slave->step_count += s; }
        }

        void enable(bool state);
        bool is_enabled() const;
        inline bool is_moving() const { return moving; };
        inline void start_moving() { moving= true; }
        inline void stop_moving() { moving= false; }

        void manual_step(bool dir);

        inline bool which_direction() const { return direction; }

        float get_steps_per_mm()  const { return steps_per_mm; }
        void change_steps_per_mm(float);
        void change_last_milestone(float);
        void set_last_milestones(float, int32_t);
        void update_last_milestones(float mm, int32_t steps);
        float get_last_milestone(void) const { return last_milestone_mm; }
        int32_t get_last_milestone_steps(void) const { return last_milestone_steps; }
        float get_current_position(void) const { return (float)(step_count-step_count_homed)/steps_per_mm; }
        int32_t get_current_step_position(void) const { return step_count-step_count_homed; }
        int32_t get_step_count(void) const { return step_count; }
        float get_max_rate(void) const { return max_rate; }
        void set_max_rate(float mr) { max_rate= mr; }
        void set_acceleration(float a) { acceleration= a; }
        float get_acceleration() const { return acceleration; }
        bool is_selected() const { return selected; }
        void set_selected(bool b) { selected= b; }
        bool is_extruder() const { return extruder; }
        void set_extruder(bool b) { extruder= b; }

        int32_t steps_to_target(float);

        bool has_slave() const { return p_slave != nullptr; }
        StepperMotor *get_slave() const { return p_slave; }
        bool init_slave(StepperMotor *sm);

        int32_t get_backlash_steps(int32_t st);
        float get_backlash_mm() const { return backlash_mm; }
        void set_backlash_mm(float bl) { backlash_mm = bl; }
        void enable_backlash(bool flg) { backlash_enabled = flg; }
        bool get_backlash_enabled() const { return backlash_enabled; }

    private:

        Pin step_pin;
        Pin dir_pin;
        Pin en_pin;

        // if this motor has a slave the pointer to it is set here
        StepperMotor *p_slave{nullptr};

        float steps_per_mm;
        float max_rate; // this is not really rate it is in mm/sec, misnamed used in Robot and Extruder
        float acceleration;

        volatile int32_t step_count;
        int32_t step_count_homed;
        int32_t last_milestone_steps;
        float   last_milestone_mm;
        float   backlash_mm;

        volatile struct {
            uint8_t motor_id:8;
            volatile bool direction:1;
            volatile bool moving:1;
            volatile uint8_t last_direction:2;
            bool selected:1;
            bool extruder:1;
            bool backlash_enabled:1;
        };

#ifdef DRIVER_TMC
    public:
        bool set_current(float c);
        bool set_microsteps(uint16_t ms);
        int get_microsteps();
        bool setup_tmc(ConfigReader& cr, const char *, uint32_t type);
        void dump_status(OutputStream& os, bool flag=true);
        void set_raw_register(OutputStream& os, uint32_t reg, uint32_t val);
        bool set_options(GCode& gcode);
        bool check_driver_error();
        static bool set_vmot(bool state) { bool last= vmot; vmot= state; return last; }
        static bool get_vmot() { return vmot; }
        void set_vmot_lost() { vmot_lost= true; if(p_slave!=nullptr) p_slave->vmot_lost= true; }

    private:
        uint32_t current_ma{0};
        uint32_t tmc_type{0};
        // TMCxxxx driver
        TMCBase *tmc{nullptr};
        static bool vmot;
        bool vmot_lost{true};
#endif
};

