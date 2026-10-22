#include "mpg.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Dispatcher.h"
#include "GCode.h"
#include "RotaryEncoder.h"
#include "Pin.h"
#include "OutputStream.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "Conveyor.h"

#include <cmath>

#define enable_key "enable"
#define enca_pin_key "enca_pin"
#define encb_pin_key "encb_pin"
#define mmpp_key "mmperpulse"

REGISTER_MODULE(MPG, MPG::create)

bool MPG::create(ConfigReader& cr)
{
    printf("DEBUG: configure MPG(s)\n");

    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("mpg", ssmap)) {
        printf("INFO: configure-mpg: no mpg section found\n");
        return false;
    }

    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach mpg
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, enable_key, false)) {
            MPG *t = new MPG(name.c_str());
            if(t->configure(cr, m, name)) {
                ++cnt;
            } else {
                printf("WARNING: failed to configure MPG %s\n", name.c_str());
                delete t;
            }
            if(name == "shared") {
                if(cnt > 1) {
                    printf("ERROR: Cannot mix shared and axis MPGs\n");
                    delete t;
                }
                break;
            }
        }
    }

    printf("INFO: %d mpg(s) loaded\n", cnt);
    return cnt > 0;
}

MPG::MPG(const char *name) : Module("mpg", name)
{
}

// configure each instance of MPG, one instance per axis under control,
// unless shared in which case there can only be one
bool MPG::configure(ConfigReader& cr, ConfigReader::section_map_t& m, const std::string& name)
{
    // foreach axis, name needs to be x,y,z,a,b,c or shared
    // check it is a valid axis designation
    if(name == "shared") {
        shared = true;
        axis = -1; // no axis selected
    } else {
        shared = false;
        if(name.size() != 1 || name.find_first_of("xyzabc") == name.npos) {
            printf("ERROR: configure-dro: axis %s is not one of xyzabc or shared\n", name.c_str());
            return false;
        }
        // convert to axis 0-5
        char c = name[0];
        int n = c - 'x';
        if(n < 0) n = c - 'a';
        if(n >= X_AXIS && n < Robot::getInstance()->get_number_registered_motors()) {
            axis = n;
        } else {
            printf("ERROR: configure-dro: illegal axis %s, %d\n", name.c_str(), n);
            return false;
        }

        // see if a default mm per pulse is defined
        mm_per_pulse = cr.get_float(m, mmpp_key, -1);
        if(mm_per_pulse <= 0) {
            // calculate the default by using the minimum resolution of the axis to 4 dp
            float spmm = Robot::getInstance()->actuators[axis]->get_steps_per_mm();
            mm_per_pulse = 1.0F / spmm;
            // round to 4dp
            float f = mm_per_pulse + 0.000055555F; // round up 4dp
            mm_per_pulse = ((float)((int)(f * 10000.0F))) / 10000.0F; // take 4 dp and truncate
        }
        printf("INFO: configure-mpg %s: mm per pulse is set to %1.4f\n", name.c_str(), mm_per_pulse);
    }

    // pin1 and pin2 must be interrupt capable pins that have not already got interrupts assigned for that line number
    Pin *pin1, *pin2;
    pin1 = new Pin(cr.get_string(m, enca_pin_key , "nc"));
    pin2 = new Pin(cr.get_string(m, encb_pin_key , "nc"));

    enc = new RotaryEncoder(*pin1, *pin2, std::bind(&MPG::check_encoder, this));
    if(!enc->setup()) {
        printf("ERROR: configure-mpg %s: enca and/or encb pins are not valid interrupt pins\n", name.c_str());
        delete pin1;
        delete pin2;
        delete enc;
        return false;
    }


// reset encoder state for new axis
         last_count = enc->get_count();
        delta_change.store(0);

    // set this so the command ctx ll back gets called
    want_command_ctx = true;

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 922, std::bind(&MPG::set_ppmm, this, _1, _2));

    return true;
}

// set the distance to move per pulse via a M922 code
bool MPG::set_ppmm(GCode& gcode, OutputStream& os)
{
    char a;
    if(shared) {
        if(gcode.get_num_args() > 1) {
            os.printf("only one axis can be specified\n");
            return false;
        }

        if(gcode.get_num_args() == 0) {
            axis = -1; // disable all axis
            return true;
        }

        if(gcode.has_arg('X')) { a = 'X'; axis = X_AXIS; }
        else if(gcode.has_arg('Y')) { a = 'Y'; axis = Y_AXIS; }
        else if(gcode.has_arg('Z')) { a = 'Z'; axis = Z_AXIS; }
        else if(gcode.has_arg('A')) { a = 'A'; axis = A_AXIS; }
        else if(gcode.has_arg('B')) { a = 'B'; axis = B_AXIS; }
        else if(gcode.has_arg('C')) { a = 'C'; axis = C_AXIS; }
        else {
            os.printf("Arg must be one of XYZABC");
            return false;
        }

        // reset encoder state for new axis
        last_count = enc->get_count();
        delta_change.store(0);

    } else {
        if(axis >= X_AXIS && axis <= Z_AXIS) {
            a = 'X' + axis;
        } else if(axis >= A_AXIS && axis <= C_AXIS) {
            a = 'A' + axis - A_AXIS;
        } else {
            return false;
        }
    }

    if(gcode.has_arg(a)) {
        mm_per_pulse = gcode.get_arg(a); // distance in mm per pulse
        if(mm_per_pulse <= 0) {
            os.printf("mm per pulse (%f) for axis %c must be > 0.0\n", mm_per_pulse, a);
            if(shared) axis = -1; // disable axis as invalid mm per pulse
        } else {
            os.printf("mm per pulse for axis %c = %f\n", a, mm_per_pulse);
        }
        return true;
    }

    return false;
}

// this gets called in command thread to issue the delta_move()
void MPG::in_command_ctx(bool idle)
{
    if(axis == -1) return; // all axis disabled

    // get any accumulated encoder movement
    int32_t d = delta_change.exchange(0);
    if(d == 0) return;

    int n_motors = Robot::getInstance()->get_number_registered_motors();
    if(axis >= n_motors) return;

    float delta[n_motors];
    for (int i = 0; i < n_motors; ++i) {
        delta[i] = NAN;
    }

    // amount to move
    delta[axis] = d * mm_per_pulse;
    // always move at the maximum rate for the axis
    float fr = Robot::getInstance()->actuators[axis]->get_max_rate();
    Robot::getInstance()->delta_move(delta, fr, n_motors);
}

// interrupt handler
void MPG::check_encoder()
{
    uint32_t cnt = enc->get_count();
    // handle wrap around
    uint32_t qemax = 0XFFFFFFFF;
    uint32_t delta = 0;
    int sign = 1;

    // handle encoder wrap around and get encoder pulses since last read
    if(cnt < last_count && (last_count - cnt) > (qemax / 2)) {
        delta = (qemax - last_count) + cnt + 1;
        sign = 1;
    } else if(cnt > last_count && (cnt - last_count) > (qemax / 2)) {
        delta = (qemax - cnt) + last_count + 1;
        sign = -1;
    } else if(cnt > last_count) {
        delta = cnt - last_count;
        sign = 1;
    } else if(cnt < last_count) {
        delta = last_count - cnt;
        sign = -1;
    }
    last_count = cnt;

    int32_t d = sign * delta;

    if(d != 0) {
        delta_change.fetch_add(d);
    }
}

/*
    example config.ini entry:-

    [mpg]
    x.enable = true
    x.enca_pin = PF10^  # must be an interrupt pin that the line number (10) is unused
    x.encb_pin = PF6^   # must be an interrupt pin that the line number (6) is unused
    x.mmperpulse = 0.01 # optionally set the default mm per pulse to move defaults to resolution of axis

    y.enable = true
    y.enca_pin = PA3^
    y.encb_pin = PA4^

    OR
    shared.enable = true
    shared.enca_pin = PF10^  # must be an interrupt pin that the line number (10) is unused
    shared.encb_pin = PF6^   # must be an interrupt pin that the line number (6) is unused
    # then an axis is enabled by setting the mmperpulse with M922 X0.01
    # it can be disabled with M922 and no arguments
    # otherwise it stays enabled until a different axis is enabled
*/
