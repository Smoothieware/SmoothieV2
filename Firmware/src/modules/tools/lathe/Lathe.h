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
        float get_rpm() const { return rpm; }

    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        bool rpm_cmd(std::string& params, OutputStream& os);
        void update_position();
        float calculate_position(int32_t cnt);
        int32_t get_encoder_delta();
        void handle_rpm();

        float wanted_pos{0};
        StepperMotor *stepper_motor;
        uint8_t motor_id;

        int32_t last_cnt{0};
        float delta_mm;
        float distance;
        float start_pos;
        float dpr; // distance per rotation set by K
        float ppr;  // encoder pulses per rotation
        float target_position{0}; // position in mm we want the Z axis to move
        float rpm{0};
        volatile bool running{false};
};
