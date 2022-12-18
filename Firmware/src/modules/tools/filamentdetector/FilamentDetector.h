#pragma once

#include "Module.h"
#include "Pin.h"

#include <atomic>
#include <string>

class OutputStream;
class GCode;

class FilamentDetector: public Module
{
public:
    FilamentDetector();
    ~FilamentDetector();
    static bool create(ConfigReader& cr);
    bool configure(ConfigReader& cr);
    virtual void in_command_ctx(bool);
    virtual void on_halt(bool flg);

private:
    bool handle_mcode(GCode& gcode, OutputStream& os);

    void on_pin_rise();
    void check_encoder();
    void button_tick();
    float get_emove();
    bool is_suspended() const;
    void on_second_tick();

    Pin *encoder_pin{0};
    Pin *bulge_pin;
    Pin *detector_pin;
    float e_last_moved{0};
    std::atomic_uint pulses{0};
    float pulses_per_mm{0};
    uint8_t seconds_per_check{1};
    uint8_t seconds_passed{0};

    struct {
        bool filament_out_alarm:1;
        bool bulge_detected:1;
        bool active:1;
        bool was_retract:1;
        bool leave_heaters_on:1;
        bool triggered:1;
    };
};
