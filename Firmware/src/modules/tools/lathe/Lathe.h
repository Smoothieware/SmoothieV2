#pragma once

#include "Module.h"

class GCode;
class OutputStream;
class StepperMotor;
class Pin;

class Lathe : public Module {
    public:
        Lathe();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);
        virtual void on_halt(bool flg);
        float get_rpm() const { return rpm; }
        bool is_running() const { return running; }
        float get_distance() const { return distance; }

    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        bool rpm_cmd(std::string& params, OutputStream& os);
        int update_position();
        float calculate_position(int32_t cnt);
        float get_encoder_delta();
        void handle_rpm();
        void handle_rpm_encoder(uint32_t deltams);
        void handle_index_irq();

        float wanted_pos{0};
        StepperMotor *stepper_motor;
        Pin *index_pin{nullptr};
        volatile uint32_t index_pulse{0};

        uint8_t motor_id;
        bool current_direction;
        float delta_mm;
        float distance;
        float start_pos;
        float dpr; // distance per rotation set by K
        float ppr;  // encoder pulses per rotation
        float target_position{0}; // position in mm we want the Z axis to move
        float rpm{0};
        volatile bool running{false};
        bool started{false};
};
