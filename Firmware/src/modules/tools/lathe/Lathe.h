#pragma once

#include "Module.h"
#include "Pin.h"

class GCode;
class OutputStream;
class StepperMotor;

class Lathe : public Module {
    public:
        Lathe();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);
        virtual void on_halt(bool flg);

    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        void update_position();
        float calculate_position(int32_t cnt);
        float get_encoder_delta();

        float wanted_pos{0};
        StepperMotor *stepper_motor;
        uint8_t motor_id;

        int32_t last_cnt{0};
        float distance;
        float start_pos;
        float dpr; // distance per rotation set by K
        float ppr;  // encoder pulses per rotation
        float target_position; // position in mm we want the Z axis to move
        volatile bool running{false};
};
