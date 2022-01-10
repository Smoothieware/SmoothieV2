#pragma once

#include "Module.h"
#include "ConfigReader.h"
#include "Pin.h"

#include <string>
#include <set>

class GCode;
class OutputStream;
class SigmaDeltaPwm;
class Pwm;

class Switch : public Module {
    public:
        Switch(const char *name);

        static bool load_switches(ConfigReader& cr);

        void on_halt(bool);
        virtual void in_command_ctx(bool);
        bool request(const char *key, void *value) ;
        bool is_output() const { return !is_input; }

        enum OUTPUT_TYPE {NONE, SIGMADELTA, DIGITAL, HWPWM};
        std::string get_info() const;

    private:
        bool configure(ConfigReader& cr, ConfigReader::section_map_t& m);
        static void pinpoll_tick(void);

        bool handle_gcode(GCode& gcode, OutputStream& os);
        void handle_switch_changed();
        bool match_input_on_gcode(const GCode& gcode) const;
        bool match_input_off_gcode(const GCode& gcode) const;

        static std::set<Switch*> input_list;
        float switch_value;
        float default_on_value;
        OUTPUT_TYPE output_type;
        union {
            Pin *input_pin;
            Pin *digital_pin;
            SigmaDeltaPwm *sigmadelta_pin;
            Pwm *pwm_pin;
        };
        std::string output_on_command;
        std::string output_off_command;
        enum {momentary_behavior, toggle_behavior};
        uint16_t  input_pin_behavior;
        uint16_t  input_on_command_code;
        uint16_t  input_off_command_code;
        struct {
            char input_on_command_letter:8;
            char input_off_command_letter:8;
            uint8_t subcode:8;
            bool ignore_on_halt:1;
            bool halt_setting:1;
            bool failsafe:1;
            bool input_pin_state:1;
            bool switch_state:1;
            bool is_input:1;
        };
};

