#include "Lathe.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Dispatcher.h"
#include "GCode.h"
#include "StepTicker.h"
#include "SlowTicker.h"
#include "QuadEncoder.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "main.h"
#include "OutputStream.h"
#include "TM1638.h"
#include "buttonbox.h"

#include <cmath>

#define enable_key "enable"
#define ppr_key "encoder_ppr"
#define display_rpm_key "display_rpm"
#define use_buttons_key "use_buttons"

#define RPM_UPDATE_HZ 10

// define a map of button names and but position
// NOTE thee buttons need to be defined in button box config as well
static std::map<std::string, uint8_t> button_names = {
    {"lathe-b1", 0x01},
    {"lathe-b2", 0x02},
    {"lathe-b3", 0x04},
    {"lathe-b4", 0x08},
    {"lathe-b5", 0x10},
    {"lathe-b6", 0x20},
    {"lathe-b7", 0x40},
    {"lathe-b8", 0x80},
};


REGISTER_MODULE(Lathe, Lathe::create)

bool Lathe::create(ConfigReader& cr)
{
    printf("DEBUG: configure Lathe\n");
    Lathe *t = new Lathe();
    if(!t->configure(cr)) {
        printf("INFO: Lathe not enabled\n");
        delete t;
    }
    return true;
}

Lathe::Lathe() : Module("Lathe")
{
    distance= NAN;
}

bool Lathe::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("lathe", m)) return false;

    bool enable = cr.get_bool(m,  enable_key , false);
    if(!enable) {
        return false;
    }

    if(!setup_quadrature_encoder()) {
        printf("ERROR: configure-lathe: unable to setup quadrature encoder\n");
        return false;
    }

    // pulses per rotation (takes into consideration any gearing) ppr= encoder resolution * gear ratio
    ppr = cr.get_float(m, ppr_key, 1000);
    printf("INFO: configure-lathe: encoder ppr %f\n", ppr);

    // Actuator that is synchronized with the spindle
    // on a Lathe Z is the leadscrew for the carriage, X is the cross carriage
    // TODO needs to be configurable
    stepper_motor = Robot::getInstance()->actuators[Z_AXIS];
    motor_id = stepper_motor->get_motor_id();
    if(motor_id != Z_AXIS) {
        // error registering, maybe too many
        printf("ERROR: configure-lathe: unable to get stepper motor\n");
        return false;
    }

    // what is the step accuracy in mm to 4 decimal places rounded up
    delta_mm = roundf((1.0F / stepper_motor->get_steps_per_mm()) * 10000.0F) / 10000.0F;

    // use the display and buttons
    display_rpm = cr.get_bool(m, display_rpm_key, false);

    if(display_rpm) {
        // can only use buttons if the display is defined
        use_buttons = cr.get_bool(m, use_buttons_key, false);

        // register a startup function that will be called after all modules have been loaded
        // (as this module relies on the tm1638 module having been loaded)
        register_startup(std::bind(&Lathe::after_load, this));
    }

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 33, std::bind(&Lathe::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler("rpm", std::bind( &Lathe::rpm_cmd, this, _1, _2) );

    SlowTicker::getInstance()->attach(RPM_UPDATE_HZ, std::bind(&Lathe::handle_rpm, this));

    if(use_buttons) {
        SlowTicker::getInstance()->attach(100, std::bind(&Lathe::check_buttons, this));
    }

    return true;
}

void Lathe::after_load()
{
    // get display if available
    Module *v= Module::lookup("tm1638");
    if(v != nullptr) {
        TM1638 *tm=  static_cast<TM1638*>(v);
        display= tm;
        tm->displayBegin();
        tm->reset();
        printf("DEBUG: lathe: display started\n");
    }else{
        printf("WARNING: lathe: display is not available\n");
        display_rpm= false;
    }

    if(use_buttons) {
        // button box is used to define button functions
        v= Module::lookup("buttonbox");
        if(v != nullptr) {
            ButtonBox *bb=  static_cast<ButtonBox*>(v);
             // assign a callback for each of the buttons
            // we need to match button names here with what was defined in button box config
            using std::placeholders::_1;
            for(auto i : button_names) {
                if(!bb->set_cb_fnc(i.first.c_str(), std::bind(&Lathe::check_button, this, _1))){
                    printf("WARNING: lathe: button %s was not defined in button box\n", i.first.c_str());
                }
            }
        }else{
            printf("WARNING: lathe: button box is not available\n");
            use_buttons= false;
        }
    }
}

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }
bool Lathe::rpm_cmd(std::string& params, OutputStream& os)
{
    HELP("display current rpm");

    os.printf("%1.1f\n", rpm);
    os.set_no_response();
    return true;
}

bool Lathe::handle_gcode(GCode& gcode, OutputStream& os)
{
    int code = gcode.get_code();

    if(code == 33) {
        if(gcode.has_arg('K')) {
            dpr = gcode.get_arg('K'); // distance per revolution
            if(dpr == 0) {
                gcode.set_error("K argument must be > 0");
                return true;
            }

        } else {
            gcode.set_error("K argument required");
            return true;
        }

        if(gcode.has_arg('Z')) {
            if(rpm == 0) {
                gcode.set_error("Spindle must be running");
                return true;
            }

            distance = gcode.get_arg('Z'); // distance to move
            start_pos = stepper_motor->get_current_position();
            running = true;

            // We have to wait for this to complete
            while(running && !Module::is_halted()) {
                safe_sleep(100);
                // update DROs occasionally
                Robot::getInstance()->reset_position_from_current_actuator_position();
                if(rpm == 0) {
                    os.printf("error: Spindle stopped running\n");
                    broadcast_halt(true);
                    break;
                }
            }

            // reset the position based on current actuator position
            Robot::getInstance()->reset_position_from_current_actuator_position();

        } else if(gcode.has_arg('X') || gcode.has_arg('Y')) {
            gcode.set_error("Only (Lathe) Z axis currently supported");

        } else {
            // no Z arg means manual mode where the half nut must be engaged and disengaged, control Y will stop it
            // K sets the mm per revolution
            distance = NAN;
            target_position = stepper_motor->get_current_position();
            if(!stepper_motor->is_enabled()) stepper_motor->enable(true);
            current_direction= stepper_motor->get_direction();

            // have stepticker call us
            StepTicker::getInstance()->callback_fnc= std::bind(&Lathe::update_position, this);
            running = true;

            while(!os.get_stop_request() && !Module::is_halted()) {
                safe_sleep(100);
                // update DROs occasionally
                Robot::getInstance()->reset_position_from_current_actuator_position();
                //printf("%f %ld\n", target_position, read_quadrature_encoder());
                // if(rpm == 0) {
                //     // also stop if spindle stops
                //     break;
                // }
            }
            running = false;

            StepTicker::getInstance()->callback_fnc= nullptr;

            os.set_stop_request(false);
            safe_sleep(100);
            // reset the position based on current actuator position
            Robot::getInstance()->reset_position_from_current_actuator_position();
        }

        return true;
    }

    // if not handled
    return false;
}

// called every 100 ms to calculate current RPM
// Note at .5 secs sample rate we would wrap the counter at 960RPM and get a false reading (with a 2000ppr encoder returning 4000ppr)
// at 10Hz sample rate we can go upto 4500RPM without wrapping
// using a moving average to steady the RPM reading
void Lathe::handle_rpm()
{
    static float ave[10];
    static int ave_cnt= 0;
    static uint32_t iter= 0;
    static uint32_t last = 0;
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t qediv= get_quadrature_encoder_div();
    uint32_t cnt = read_quadrature_encoder();
    uint32_t c = (cnt > last) ? cnt - last : last - cnt;
    last = cnt;

    // deal with over/underflow
    if(c > qemax / 2 ) {
        c = qemax - c + 1;
    }
    // get RPM
    float r = (c * 60 * RPM_UPDATE_HZ) / (ppr * qediv);

    if(ave_cnt < 10) {
        // fill the array first
        ave[ave_cnt++]= r;
        rpm= r;
    }else{
        // use moving average
        float sum= 0;
        for (int i = 0; i < 10-1; ++i) {
            sum += ave[i];
            ave[i]= ave[i+1];
        }
        ave[9]= r;
        rpm= sum/10;
    }

    if(display_rpm) {
        // update display once per second
        if(++iter >= RPM_UPDATE_HZ) {
            TM1638 *tm=  static_cast<TM1638*>(display);
            tm->displayIntNum((int)roundf(rpm), false, TMAlignTextRight);
            if(running) {
                tm->setLED(std::isnan(distance)?0:1, 1);
            }else{
                tm->setLED(std::isnan(distance)?0:1, 0);
            }
            iter= 0;
        }
    }
}

// given move in spindle, calculate where the controlled axis should be
float Lathe::calculate_position(int32_t cnt)
{
    // TODO calculate position given counts per rotation
    // TODO fixed 100 cpr needs to be set in config though
    float mm_per_rotation = 1.0F;
    return cnt / 100.0F * mm_per_rotation;
}

#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

// As these are called from the stepticker put them in RAM for faster execution
// return true if a and b are within the delta range of each other
_ramfunc_
static bool equal_within(float a, float b, float delta)
{
    float diff = a - b;
    if (diff < 0) diff = -diff;
    if (delta < 0) delta = -delta;
    return (diff <= delta);
}

_ramfunc_
float Lathe::get_encoder_delta()
{
    static uint32_t last_cnt = 0;
    float delta= 0;
    uint32_t cnt = read_quadrature_encoder();
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t qediv= get_quadrature_encoder_div();
    int sign= 1;

    // handle encoder wrap around and get encoder pulses since last read
    if(cnt < last_cnt && (last_cnt - cnt) > (qemax / 2)) {
        delta = (qemax - last_cnt) + cnt + 1;
        sign= 1;
    } else if(cnt > last_cnt && (cnt - last_cnt) > (qemax / 2)) {
        delta = (qemax - cnt) + last_cnt + 1;
        sign= -1;
    } else if(cnt > last_cnt) {
        delta = cnt - last_cnt;
        sign = 1;
    }else if(cnt < last_cnt){
        delta = last_cnt - cnt;
        sign = -1;
    }
    last_cnt = cnt;

    return (sign * delta) / qediv;
}

// called from stepticker every 5us
// @2000RPM that is an encoder pulse (2000ppr) every 15us.
// Depending on steps/mm the chances are good that only one step will keep up with the spindle at this callback rate
// if not then two or more steps maybe issued at a very fast rate
// NOTE We could run this at a much slower rate and try to setup a block to move the distance accumulated, at a rate
// determined by the spindle RPM. Not sure if that is practical though.
_ramfunc_
int Lathe::update_position()
{
    if(!running || Module::is_halted()) return -1;

    if(std::isnan(distance)) {
        // just run the lead screw at the given rate (mm/rev in dpr) until told to stop

        float delta= get_encoder_delta();

        if(delta != 0) {
            // calculate fraction of a rotation since last time and based on dpr calculate how far to move
            float mm = dpr * (delta / ppr); // calculate mm to move based on requested distance per rev
            target_position += mm; // accumulate target move
        }

        float current_position = stepper_motor->get_current_position();

        // we first check if the target is equal to current within the limits of the step increment
        if(!equal_within(target_position, current_position, delta_mm)) {
            if(target_position > current_position) {
                if(current_direction) {
                    stepper_motor->set_direction(false);
                    current_direction= false;
                }
                stepper_motor->step();
                return motor_id;
            } else if(target_position < current_position) {
                if(!current_direction) {
                    stepper_motor->set_direction(true);
                    current_direction= true;
                }
                stepper_motor->step();
                return motor_id;
            }
        }
        return -1;
    }

#if 0
    else{
        // TODO this is not G33 mode, currently G33 just turns on the synchronized spindle mode
        // which will travel for the given distance before suddenly stopping

        // get current spindle position
        float delta = get_encoder_delta();
        wanted_pos = calculate_position(delta);

        // compare with current position and issue step if needed
        float current_position = stepper_motor->get_current_position();
        if(current_position >= start_pos + distance) {
            running = false;
            return;
        }

        if(wanted_pos > current_position) {
            stepper_motor->manual_step(true);

        } else if(wanted_pos < current_position) {
            stepper_motor->manual_step(false);
        }
    }
#endif

    return -1;
}

// called in slow timer every 100ms to scan buttons
void Lathe::check_buttons()
{
    TM1638 *tm=  static_cast<TM1638*>(display);
    buttons = tm->readButtons();
    /* buttons contains a byte with values of button s8s7s6s5s4s3s2s1
     HEX  :  Switch no : Binary
     0x01 : S1 Pressed  0000 0001
     0x02 : S2 Pressed  0000 0010
     0x04 : S3 Pressed  0000 0100
     0x08 : S4 Pressed  0000 1000
     0x10 : S5 Pressed  0001 0000
     0x20 : S6 Pressed  0010 0000
     0x40 : S7 Pressed  0100 0000
     0x80 : S8 Pressed  1000 0000
    */
}

bool Lathe::check_button(const char *name)
{
    auto bm= button_names.find(name);
    if(bm != button_names.end()) {
        return (buttons & bm->second) != 0;
    }

    return false;
}

void Lathe::on_halt(bool flg)
{
    if(flg) {
        running= false;
    }
}
