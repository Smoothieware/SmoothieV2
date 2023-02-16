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

    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        void update_position();
        float calculate_position(int32_t cnt);

        float wanted_pos{0};
        StepperMotor *stepper_motor;
        uint8_t motor_id;
};
