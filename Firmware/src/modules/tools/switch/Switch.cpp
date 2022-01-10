#include "Switch.h"

#include "GCode.h"
#include "OutputStream.h"
#include "ConfigReader.h"
#include "SlowTicker.h"
#include "FastTicker.h"
#include "SigmaDeltaPwm.h"
#include "Pwm.h"
#include "GCodeProcessor.h"
#include "Dispatcher.h"
#include "Conveyor.h"
#include "main.h"
#include "Consoles.h"

#include <algorithm>
#include <math.h>
#include <string.h>

#define startup_state_key       "startup_state"
#define startup_value_key       "startup_value"
#define default_on_value_key    "default_on_value"
#define input_pin_key           "input_pin"
#define input_pin_behavior_key  "input_pin_behavior"
#define command_subcode_key     "subcode"
#define input_on_command_key    "input_on_command"
#define input_off_command_key   "input_off_command"
#define output_pin_key          "output_pin"
#define output_type_key         "output_type"
#define max_pwm_key             "max_pwm"
#define output_on_command_key   "output_on_command"
#define output_off_command_key  "output_off_command"
#define failsafe_key            "failsafe_set_to"
#define halt_setting_key        "halt_set_to"
#define ignore_onhalt_key       "ignore_on_halt"

#define ROUND2DP(x) ((int)roundf((x) * 100.0F))

// register this module for creation in main
REGISTER_MODULE(Switch, Switch::load_switches)

std::set<Switch*> Switch::input_list;

bool Switch::load_switches(ConfigReader& cr)
{
    printf("DEBUG: configure switches\n");
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("switch", ssmap)) {
        printf("configure-switch: no switch section found\n");
        return false;
    }

    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach switch
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, "enable", false)) {
            Switch *sw = new Switch(name.c_str());
            if(sw->configure(cr, m)){
                ++cnt;
            }else{
                printf("configure-switch: failed to configure switch %s\n", name.c_str());
                delete sw;
            }
        }
    }

    printf("INFO: %d switche(s) loaded\n", cnt);

    return cnt > 0;
}

Switch::Switch(const char *name) : Module("switch", name)
{ }

bool Switch::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    // see if input pin
    std::string nm= cr.get_string(m, input_pin_key, "");
    if(!nm.empty()) {
        // it is an input
        input_pin= new Pin(nm.c_str(), Pin::AS_INPUT);
        if(!input_pin->connected()) {
            delete input_pin;
            printf("ERROR: switch-config - illegal pin: %s\n", nm.c_str());
            return false;
        }

        std::string ipb = cr.get_string(m, input_pin_behavior_key, "momentary");
        input_pin_behavior = (ipb == "momentary") ? momentary_behavior : toggle_behavior;
        output_on_command = cr.get_string(m, output_on_command_key, "");
        output_off_command = cr.get_string(m, output_off_command_key, "");
        // for commands we may need to replace _ for space for old configs
        std::replace(output_on_command.begin(), output_on_command.end(), '_', ' '); // replace _ with space
        std::replace(output_off_command.begin(), output_off_command.end(), '_', ' '); // replace _ with space

        // set to initial state
        input_pin_state = input_pin->get();
        if(input_pin_behavior == momentary_behavior) {
            // initialize switch state to same as current pin level
            switch_state = input_pin_state;
        }

        is_input= true;
        output_type= NONE;

        // setup input pin polling
        input_list.insert(this);
        if(input_list.size() == 1) {
            // we only have one of these in Switch and it is setup on first entry
            SlowTicker::getInstance()->attach(50, Switch::pinpoll_tick);
        }
        return true;
    }

    is_input= false;

    // it is an output pin of some sort
    subcode = cr.get_int(m, command_subcode_key, 0);
    std::string input_on_command = cr.get_string(m, input_on_command_key, "");
    std::string input_off_command = cr.get_string(m, input_off_command_key, "");
    if(input_on_command.empty() && input_off_command.empty()) {
        printf("ERROR: switch-config - output pin has no command to trigger it\n");
        return false;
    }

    switch_state = cr.get_bool(m, startup_state_key, false);
    failsafe = cr.get_bool(m, failsafe_key, false);
    halt_setting = cr.get_bool(m, halt_setting_key, false);
    ignore_on_halt = cr.get_bool(m, ignore_onhalt_key, false);


    // output pin type
    std::string type = cr.get_string(m, output_type_key, "");
    if(type != "sigmadeltapwm" && type != "digital" && type != "hwpwm") {
        printf("ERROR: switch-config - output pin type is not valid: %s\n", type.c_str());
        return false;
    }

    std::string output_pin = cr.get_string(m, output_pin_key, "nc");

    if(type == "sigmadeltapwm") {
        output_type = SIGMADELTA;
        sigmadelta_pin = new SigmaDeltaPwm(output_pin.c_str());
        if(!sigmadelta_pin->connected()) {
            printf("ERROR: switch-config - Selected sigmadelta pin is invalid\n");
            delete sigmadelta_pin;
            return false;
        }
        if(failsafe) {
            //set_high_on_debug(sigmadelta_pin->port_number, sigmadelta_pin->pin);
        } else {
            //set_low_on_debug(sigmadelta_pin->port_number, sigmadelta_pin->pin);
        }

    } else if(type == "digital") {
        output_type = DIGITAL;
        digital_pin = new Pin(output_pin.c_str(), Pin::AS_OUTPUT);
        if(!digital_pin->connected()) {
            printf("ERROR: switch-config - Selected digital pin is invalid\n");
            delete digital_pin;
            return false;

        } else {
            if(failsafe) {
                //set_high_on_debug(digital_pin->port_number, digital_pin->pin);
            } else {
                //set_low_on_debug(digital_pin->port_number, digital_pin->pin);
            }
        }

    } else if(type == "hwpwm") {
        output_type = HWPWM;
        pwm_pin = new Pwm(output_pin.c_str());
        if(!pwm_pin->is_valid()) {
            printf("ERROR: switch-config - Selected HWPWM output pin is not valid\n");
            delete pwm_pin;
            return false;
        }

        if(failsafe == 1) {
            //set_high_on_debug(pin->port_number, pin->pin);
        } else {
            //set_low_on_debug(pin->port_number, pin->pin);
        }
    }

    if(output_type == SIGMADELTA) {
        sigmadelta_pin->max_pwm(cr.get_int(m,  max_pwm_key, 255));
        switch_value = cr.get_int(m, startup_value_key, sigmadelta_pin->max_pwm());
        if(switch_state) {
            sigmadelta_pin->pwm(switch_value); // will be truncated to max_pwm
        } else {
            sigmadelta_pin->set(false);
        }

    } else if(output_type == HWPWM) {
        // default is 0% duty cycle
        switch_value = cr.get_float(m, startup_value_key, 0);
        default_on_value = cr.get_float(m, default_on_value_key, 0);
        if(switch_state) {
            pwm_pin->set(default_on_value/100.0F);
        } else {
            pwm_pin->set(switch_value/100.0F);
        }

    } else if(output_type == DIGITAL) {
        digital_pin->set(switch_state);
    }

    // Set the on/off command codes
    input_on_command_letter = 0;
    input_off_command_letter = 0;

    if(input_on_command.size() >= 2) {
        input_on_command_letter = input_on_command.front();
        const char *p = input_on_command.c_str();
        p++;
        std::tuple<uint16_t, uint16_t> c = GCodeProcessor::parse_code(p);
        input_on_command_code = std::get<0>(c);
        if(std::get<1>(c) != 0) {
            subcode = std::get<1>(c); // override any subcode setting
        }
        // add handler for this code
        using std::placeholders::_1;
        using std::placeholders::_2;
        THEDISPATCHER->add_handler(input_on_command_letter == 'G' ? Dispatcher::GCODE_HANDLER : Dispatcher::MCODE_HANDLER, input_on_command_code, std::bind(&Switch::handle_gcode, this, _1, _2));

    }else{
        printf("WARNING: switch-config - input_on_command is not legal\n");
    }

    if(input_off_command.size() >= 2) {
        input_off_command_letter = input_off_command.front();
        const char *p = input_off_command.c_str();
        p++;
        std::tuple<uint16_t, uint16_t> c = GCodeProcessor::parse_code(p);
        input_off_command_code = std::get<0>(c);
        if(std::get<1>(c) != 0) {
            subcode = std::get<1>(c); // override any subcode setting
        }
        using std::placeholders::_1;
        using std::placeholders::_2;
        THEDISPATCHER->add_handler(input_off_command_letter == 'G' ? Dispatcher::GCODE_HANDLER : Dispatcher::MCODE_HANDLER, input_off_command_code, std::bind(&Switch::handle_gcode, this, _1, _2));
    }else{
        printf("WARNING: switch-config - input_off_command is not legal\n");
    }

    return true;
}

std::string Switch::get_info() const
{
    std::string s;

    if(is_input) {
        s.append("INPUT:");
        s.append(input_pin->to_string());
        s.append(",");

        switch(this->input_pin_behavior) {
            case momentary_behavior: s.append("momentary,"); break;
            case toggle_behavior: s.append("toggle,"); break;
        }

        if(!output_on_command.empty()) {
            s.append("OUTPUT_ON_COMMAND:");
            s.append(output_on_command);
            s.append(",");
        }
        if(!output_off_command.empty()) {
            s.append("OUTPUT_OFF_COMMAND:");
            s.append(output_off_command);
            s.append(",");
        }

        return s;
    }

    if(digital_pin != nullptr) {
        s.append("OUTPUT:");

        switch(this->output_type) {
            case DIGITAL:
                    s.append(digital_pin->to_string());
                    s.append(",digital,");
                    break;
            case SIGMADELTA:
                    s.append(sigmadelta_pin->to_string());
                    s.append(",sigmadeltapwm,");
                    break;
            case HWPWM:
                    s.append(pwm_pin->to_string());
                    s.append(":");
                    s.append(std::to_string(pwm_pin->get()*100));
                    s.append(",hwpwm,");
                    break;
            case NONE: s.append("???,none,"); break;
        }
    }

    if(input_on_command_letter) {
        s.append("INPUT_ON_COMMAND:");
        s.push_back(input_on_command_letter);
        s.append(std::to_string(input_on_command_code));
        s.append(",");
    }
    if(input_off_command_letter) {
        s.append("INPUT_OFF_COMMAND:");
        s.push_back(input_off_command_letter);
        s.append(std::to_string(input_off_command_code));
        s.append(",");
    }
    if(subcode != 0) {
        s.append("SUBCODE:");
        s.append(std::to_string(subcode));
        s.append(",");
    }

    return s;
}

// set the pin to the halt setting value on halt
void Switch::on_halt(bool flg)
{
    if(flg) {
        if(this->ignore_on_halt) return;

        // set pin to halt value
        switch(this->output_type) {
            case DIGITAL: this->digital_pin->set(this->halt_setting); break;
            case SIGMADELTA: this->sigmadelta_pin->set(this->halt_setting); break;
            case HWPWM: this->pwm_pin->set(switch_value/100.0F); break;
            case NONE: break;
        }
        this->switch_state = this->halt_setting;
    }
}

bool Switch::match_input_on_gcode(const GCode& gcode) const
{
    bool b = ((input_on_command_letter == 'M' && gcode.has_m() && gcode.get_code() == input_on_command_code) ||
              (input_on_command_letter == 'G' && gcode.has_g() && gcode.get_code() == input_on_command_code));

    return (b && gcode.get_subcode() == this->subcode);
}

bool Switch::match_input_off_gcode(const GCode& gcode) const
{
    bool b = ((input_off_command_letter == 'M' && gcode.has_m() && gcode.get_code() == input_off_command_code) ||
              (input_off_command_letter == 'G' && gcode.has_g() && gcode.get_code() == input_off_command_code));
    return (b && gcode.get_subcode() == this->subcode);
}

// this is always called in the command thread context
bool Switch::handle_gcode(GCode& gcode, OutputStream& os)
{
    // Add the gcode to the queue ourselves if we need it
    if (!(match_input_on_gcode(gcode) || match_input_off_gcode(gcode))) {
        return false;
    }

    // TODO we need to sync this with the queue, so we need to wait for queue to empty, however due to certain slicers
    // issuing redundant switch on calls regularly we need to optimize by making sure the value is actually changing
    // hence we need to do the wait for queue in each case rather than just once at the start
    if(match_input_on_gcode(gcode)) {
        if (this->output_type == SIGMADELTA) {
            // SIGMADELTA output pin turn on (or off if S0)
            if(gcode.has_arg('S')) {
                int v = roundf(gcode.get_arg('S') * sigmadelta_pin->max_pwm() / 255.0F); // scale by max_pwm so input of 255 and max_pwm of 128 would set value to 128
                if(v != this->sigmadelta_pin->get_pwm()) { // optimize... ignore if already set to the same pwm
                    // drain queue
                    Conveyor::getInstance()->wait_for_idle();
                    this->sigmadelta_pin->pwm(v);
                    this->switch_state = (v > 0);
                }
            } else {
                // drain queue
                Conveyor::getInstance()->wait_for_idle();
                this->sigmadelta_pin->pwm(this->switch_value);
                this->switch_state = (this->switch_value > 0);
            }

        } else if (this->output_type == HWPWM) {
            // drain queue
            Conveyor::getInstance()->wait_for_idle();
            // PWM output pin set duty cycle 0 - 100
            if(gcode.has_no_args() && !(gcode.has_arg('S') || gcode.has_arg('P'))) {
                this->pwm_pin->set(this->default_on_value/100.0F);
                this->switch_state = true;

            } else {
                float v= 0;
                if(gcode.has_arg('S')) { // set duty cycle to given percentage
                    v = gcode.get_arg('S');
                    if(v > 100) v = 100;
                    else if(v < 0) v = 0;
                    this->pwm_pin->set(v / 100.0F);

                }else if(gcode.has_arg('P')) { // set pulse width to given microseconds
                    v = gcode.get_arg('P');
                    if(v < 0) v = 0;
                    v= this->pwm_pin->set_microseconds(v) * 100;
                }
                this->switch_state = (ROUND2DP(v) != ROUND2DP(this->switch_value));
            }

        } else if (this->output_type == DIGITAL) {
            // drain queue
            Conveyor::getInstance()->wait_for_idle();
            // logic pin turn on
            this->digital_pin->set(true);
            this->switch_state = true;
        }

    } else if(match_input_off_gcode(gcode)) {
        // drain queue
        Conveyor::getInstance()->wait_for_idle();
        this->switch_state = false;

        if (this->output_type == SIGMADELTA) {
            // SIGMADELTA output pin
            this->sigmadelta_pin->set(false);

        } else if (this->output_type == HWPWM) {
            this->pwm_pin->set(this->switch_value/100.0F);

        } else if (this->output_type == DIGITAL) {
            // logic pin turn off
            this->digital_pin->set(false);
        }
    }

    return true;
}

// this can be called from a timer as it only sets pins and does not issue commands
bool Switch::request(const char *key, void *value)
{
    if(strcmp(key, "state") == 0) {
        *(bool *)value = switch_state;

    } else if(strcmp(key, "value") == 0) {
        *(float *)value = switch_value;

    } else if(strcmp(key, "set-state") == 0) {
        if(!is_output()) return false;
        // TODO should we check and see if we are already in this state and ignore if we are?
        switch_state = *(bool*)value;
        if(switch_state) {
            if(output_type == SIGMADELTA) {
                sigmadelta_pin->pwm(switch_value); // this requires the value has been set otherwise it switches on to whatever it last was
            } else if (output_type == HWPWM) {
                pwm_pin->set(default_on_value / 100.0F);
            } else if (output_type == DIGITAL) {
                digital_pin->set(true);
            }
        }else{
            if(output_type == SIGMADELTA) {
                sigmadelta_pin->set(false);
            } else if (output_type == HWPWM) {
                pwm_pin->set(switch_value/100.0F);
            } else if (output_type == DIGITAL) {
                digital_pin->set(false);
            }
        }

    } else if(strcmp(key, "set-value") == 0) {
        if(!is_output()) return false;
        // TODO should we check and see if we already have this value and ignore if we do?
        switch_value = *(float*)value;
        if(output_type == SIGMADELTA) {
            sigmadelta_pin->pwm(switch_value);
            switch_state = (switch_value > 0);
        } else if (output_type == HWPWM) {
            pwm_pin->set(switch_value / 100.0F);
            switch_state = (ROUND2DP(switch_value) != ROUND2DP(switch_value));
        } else if (output_type == DIGITAL) {
            switch_state= (switch_value > 0.0001F);
            digital_pin->set(switch_state);
        }

    } else if(strcmp(key, "is_output") == 0) {
        return is_output();
    }

    return true;
}

// This is called periodically to allow commands to be issued in the command thread context
// but only when want_command_ctx is set to true
void Switch::in_command_ctx(bool idle)
{
    handle_switch_changed();
    want_command_ctx= false;
}

// Switch changed by some external means so do the action required
// needs to be only called when in command thread context
void Switch::handle_switch_changed()
{
    if(this->switch_state) {
        if(!this->output_on_command.empty()){
            OutputStream os; // null output stream
            dispatch_line(os, this->output_on_command.c_str());
        }

    } else {
        if(!this->output_off_command.empty()){
            OutputStream os; // null output stream
            dispatch_line(os, this->output_off_command.c_str());
        }
    }
}

// Check the state of any input buttons and act accordingly
// this just sets the state and lets handle_switch_changed() change the actual pins
// FIXME there is a race condition where if the button is pressed and released faster than the command loop runs then it will not see the button as active
void Switch::pinpoll_tick()
{
    for(auto inp : input_list) {
        if(!inp->is_input) continue;

        bool switch_changed = false;

        // See if pin changed
        bool current_state = inp->input_pin->get();
        if(inp->input_pin_state != current_state) {
            inp->input_pin_state = current_state;
            // If pin high
            if( inp->input_pin_state ) {
                // if switch is a toggle switch
                if( inp->input_pin_behavior == toggle_behavior ) {
                    inp->switch_state= !inp->switch_state;

                } else {
                    // else default is momentary
                    inp->switch_state = inp->input_pin_state;
                }
                switch_changed = true;

            } else {
                // else if button released
                if( inp->input_pin_behavior == momentary_behavior ) {
                    // if switch is momentary
                    inp->switch_state = inp->input_pin_state;
                    switch_changed = true;
                }
            }
        }

        if(switch_changed) {
            // we need to call handle_switch_changed but in the command thread context
            // in case there is a command to be issued
            inp->want_command_ctx= true;
        }
    }
}
