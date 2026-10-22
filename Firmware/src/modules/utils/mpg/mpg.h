#pragma once

#include "Module.h"
#include "Pin.h"
#include "ConfigReader.h"

#include <string>
#include <atomic>

class GCode;
class OutputStream;
class RotaryEncoder;

class MPG : public Module {
    public:
        MPG(const char *name);
        static bool create(ConfigReader& cr);
        virtual void in_command_ctx(bool idle);

    private:
        bool configure(ConfigReader& cr, ConfigReader::section_map_t& m, const std::string& name);
        bool handle_cmd(std::string& params, OutputStream& os);
        void check_encoder();
        bool set_ppmm(GCode& gcode, OutputStream& os);

        float mm_per_pulse;
        uint8_t axis;
        bool shared;
        RotaryEncoder *enc;
        volatile uint32_t last_count{0};
        std::atomic_int32_t delta_change;
};
