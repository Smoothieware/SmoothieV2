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
#include "Pin.h"
#include "Conveyor.h"
#include "tmr-setup.h"

#include "FreeRTOS.h"
#include "task.h"


#include <cmath>

#define enable_key "enable"
#define ppr_key "encoder_ppr"
#define index_pin_key "index_pin"
#define index_edge_key "index_edge"
#define index_debounce_key "index_debounce_us"
#define index_minimum_key "index_minimum_us"
#define qe_key "use_qe"
#define qe_pullup_key "qe_pullup"

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
    end_pos = NAN;
}

bool Lathe::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("lathe", m)) return false;

    bool enable = cr.get_bool(m,  enable_key , false);
    if(!enable) {
        return false;
    }

    index_minimum = 0; // no minimum pulse width

    // default is qe encoder
    ppr = 0;
    bool qeflg = cr.get_bool(m, qe_key, true);
    if(qeflg) {
        if(!setup_quadrature_encoder(cr.get_bool(m,  qe_pullup_key, false))) {
            printf("ERROR: configure-lathe: unable to setup quadrature encoder\n");
            return false;
        }
        // pulses per rotation (takes into consideration any gearing) ppr= encoder resolution * gear ratio
        ppr = cr.get_float(m, ppr_key, 1000);
        printf("INFO: configure-lathe: encoder ppr %f\n", ppr);
    } else {
        printf("INFO: configure-lathe: No H/W quadrature encoder\n");
    }

    // use index pin if we define one
    index_pin =  new Pin(cr.get_string(m, index_pin_key, "nc"));
    if(index_pin->connected()) {
        std::string edge = cr.get_string(m, index_edge_key, "falling");
        Pin::INT_TYPE_T e;
        if(edge == "rising") e = Pin::RISING;
        else if(edge == "falling") e = Pin::FALLING;
        else if(edge == "both") {
            e = Pin::CHANGE;
            // mimimum time of index pulse otherwise rejected in us
            index_minimum = cr.get_int(m, index_minimum_key, 0);
        }
        else {
            printf("ERROR: configure-lathe: index interrupt edge must be one of rising, falling, both: %s\n", edge.c_str());
            delete index_pin;
            return false;
        }

        // mimimum time between index pulses, in us, otherwise it will be rejected
        index_debounce = cr.get_int(m, index_debounce_key, 300);

        index_pin->as_interrupt(std::bind(&Lathe::handle_index_irq, this), e);
        if(!index_pin->connected()) {
            printf("ERROR: configure-lathe: Cannot set index pin to interrupt %s\n", index_pin->to_string().c_str());
            delete index_pin;
            index_pin = nullptr;
            return false;

        }else{
            printf("INFO: configure-lathe: using index pin: %s %s, with debounce %lu us, and minimum width %lu\n", index_pin->to_string().c_str(), edge.c_str(), index_debounce, index_minimum);
        }

    } else {
        delete index_pin;
        index_pin = nullptr;
        printf("INFO: configure-lathe: no index pin\n");

        if(!qeflg) {
            printf("ERROR: configure-lathe: At least one of qe and/or index pin must be set\n");
            return false;
        }
    }


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

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 33, std::bind(&Lathe::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler("rpm", std::bind( &Lathe::rpm_cmd, this, _1, _2) );

    SlowTicker::getInstance()->attach(10, std::bind(&Lathe::handle_rpm, this));

    return true;
}

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }
bool Lathe::rpm_cmd(std::string& params, OutputStream& os)
{
    HELP("display current rpm");
    os.printf("%1.1f\n", rpm);
    // os.printf("index: %d, encoder: %d\n", index_pulse.load(), read_quadrature_encoder());
    os.set_no_response();
    return true;
}

// return true if a and b are within the delta range of each other
template <typename T>
    bool equal_within(const T& a, const T& b, const T& delta) {
    float diff = std::abs(a - b);
    return (diff <= std::abs(delta));
}

bool Lathe::handle_gcode(GCode& gcode, OutputStream& os)
{
    int code = gcode.get_code();

    if(code == 33) {
        if(gcode.has_arg('K')) {
            dpr = gcode.get_arg('K'); // distance per revolution
            if(dpr == 0) {
                gcode.set_error("K argument cannot be 0");
                return true;
            }

            if(dpr < 0) {
                reversed = true;
                dpr = -dpr;
            } else {
                reversed = false;
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

            float distance = gcode.get_arg('Z'); // distance to move

            // if we have a qe encoder
            if(gcode.get_subcode() == 1 && ppr > 0) {
                // G33.1 uses this spindle sync method like the ELS does it.
                end_pos = stepper_motor->get_current_position() + distance;

                if(distance >= 0) {
                    reversed = true;
                } else {
                    reversed = false;
                    distance = -distance;
                }

                // if we have an index_pin then we wait to start by synchronizing to it
                // NOTE the spindle and encoder must be geared 1:1 (or multiple spindle turns per 1 encoder turn)
                // for this to have the desired effect, ie always start at the same place.
                if(index_pin != nullptr) {
                    uint32_t curindex = index_pulse.load();
                    while(curindex == index_pulse.load()) {
                        // wait for index pulse to be hit
                        // TODO may need to do safe_sleep here but that may take too long
                        if(Module::is_halted() || rpm == 0) return true;
                    }
                }

                target_position = stepper_motor->get_current_position();
                if(!stepper_motor->is_enabled()) stepper_motor->enable(true);
                current_direction = stepper_motor->get_direction();

                // have stepticker call us
                running = true;
                StepTicker::getInstance()->callback_fnc = std::bind(&Lathe::update_position, this);

                // We have to wait for this to complete
                while(running && !Module::is_halted()) {
                    safe_sleep(100);
                    if(rpm == 0) {
                        os.printf("error: Spindle stopped running\n");
                        broadcast_halt(true);
                        break;
                    }
                }
                running = false;
                end_pos = NAN;
                safe_sleep(100);
                // reset the position based on current actuator position
                Robot::getInstance()->reset_position_from_current_actuator_position();

            } else {
                // an alternative to above method, which would be better for high speeds that require acceleration/deceleration,
                // is to take the current RPM and insert as if a normal G1 Znnn Fxxx where xxx is calculated from RPM
                // this will accelerate and decelerate, but if the spindle RPM changes then the thread would be incorrect
                // for turning this may be preferred. However it is not technically moving in sync with the spindle.
                // I think this is how linuxcnc does it as only an index pulse is required to calculate RPM. IE no
                // expensive high resolution encoder is needed
                float frmms = (rpm / 60.0F) * dpr; // calculate_mmsec_from_RPM();
                float last_rpm = rpm;
                if(frmms > stepper_motor->get_max_rate()) {
                    gcode.set_error("Current Spindle RPM means feed rate will exceed maximum");
                } else {
                    if(index_pin == nullptr) {
                        gcode.set_error("Index pin is required for this function");
                    } else {
                        Conveyor::getInstance()->wait_for_idle();
                        // sync with spindle, wait for 2 revolutions and then check rpm again
                        uint32_t curindex = index_pulse.load();
                        while(curindex+2 > index_pulse.load()) {
                            // wait for index pulse to be hit
                            if(Module::is_halted() || rpm == 0) {
                                if(rpm == 0) {
                                    gcode.set_error("Spindle stopped running");
                                }
                                return true;
                            }
                        }

                        // check spindle speed is within 5% of last reading
                        if(equal_within(last_rpm, rpm, last_rpm*5/100.0F)) {
                            // issue the move, note that this will accelerate and decelerate
                            THEDISPATCHER->dispatch(os, 'G', 1, 'F', frmms*60.0F, 'Z', distance, 0);
                            Conveyor::getInstance()->wait_for_idle();
                        } else {
                            os.printf("last_rpm: %f, current rpm: %f, tolerance: %f\n", last_rpm, rpm, last_rpm*5/100.0F);
                            gcode.set_error("Spindle speed stability was not within 5%% tolerance");
                        }
                    }
                }
            }

        } else if(gcode.has_arg('X') || gcode.has_arg('Y')) {
            gcode.set_error("Only (Lathe) Z axis currently supported");

        } else if(gcode.get_subcode() == 1 && ppr > 0) {
            // REQUIRES a QE encoder
            // NOTE this may be removed as it is not standard but is usefull for testing by manually turning the spindle
            // plus it is more like the ELS way to do it.
            // no Z arg means manual mode where the half nut must be engaged and disengaged, control Y will stop it
            // K sets the mm per revolution
            end_pos = NAN;
            target_position = stepper_motor->get_current_position();
            if(!stepper_motor->is_enabled()) stepper_motor->enable(true);
            current_direction = stepper_motor->get_direction();

            // have stepticker call us
            running = true;
            StepTicker::getInstance()->callback_fnc = std::bind(&Lathe::update_position, this);

            while(!os.get_stop_request() && !Module::is_halted()) {
                safe_sleep(100);
                //printf("%f %ld\n", target_position, read_quadrature_encoder());
                // if(rpm == 0) {
                //     // also stop if spindle stops
                //     break;
                // }
            }
            running = false;
            os.set_stop_request(false);
            // give it time to fully stop
            safe_sleep(100);
            // reset the position based on current actuator position
            Robot::getInstance()->reset_position_from_current_actuator_position();

        } else {
            gcode.set_error("Z axis required or encoder required");
        }

        return true;
    }

    // if not handled
    return false;
}

extern "C" uint32_t get_microseconds();
void Lathe::handle_index_irq()
{
    // TODO add minimum pulse width if needed index_minimum > 0 (needs to interrupt on change)
    static uint32_t last_index_pulse_time = 0;
    // we need to debounce this, scope says the bounce is about 50us to 250us after the first one
    uint32_t deltaus = get_delta_microseconds(last_index_pulse_time);

    if(deltaus > index_debounce) {
        // count index pulses
        index_pulse++;
        // save time of index pulse and measure time between the pulses
        uint32_t n = get_microseconds();
        uint32_t l = index_time.exchange(n);
        uint32_t d = (n >= l) ? n-l : 0xFFFFFFFF-(l-n)+1;
        index_time_delta.store(d);
        last_index_pulse_time = get_microseconds();
    }
}

// called every 100 ms to calculate current RPM
void Lathe::handle_rpm()
{
    static uint32_t lasttime = 0;
    static uint32_t lastcnt = 0;
    if(index_pin != nullptr) {
        // measure time between index pulses, which seems to be much more stable
        uint32_t dtus = index_time_delta.load();
        if(dtus > 0) {
            // if last rpm is 0 it means the spindle was not running so last time is invalid
            // so set to 1 so the next time around we calculate the correct rpm
            if(rpm > 0) {
                rpm = 60.0F * (1e6F / dtus);
            } else {
                rpm = 1;
            }
        } else {
            rpm = 0;
            return;
        }

        // if the index does not increase within a certain time then determine the spindle has stopped
        uint32_t cnt = index_pulse.load();
        if(lastcnt == cnt) {
            uint32_t deltams = get_delta_microseconds(lasttime) / 1000;
            if(deltams > 2000) { // 2 seconds is a reasonable amount of time that would be an RPM of 30
                rpm = 0;
                index_time_delta.store(0);
            }
        } else {
            lasttime = get_microseconds();
            lastcnt = cnt;
        }

    } else if(ppr > 0) {

        if(lasttime == 0)  {
            lasttime = get_microseconds();
            return;
        }

        // use encoder to calculate RPM
        // get elapsed time since last call, more accurate than relying on 100ms timer
        uint32_t deltaus = get_delta_microseconds(lasttime);
        lasttime = get_microseconds();
        rpm = handle_rpm_encoder(deltaus/1000);
    }
}

// calculate RPM from Encoder
// each pulse is about 32us @ 1000RPM
// Note at .5 secs sample rate we would wrap the counter at 960RPM and get a false reading (with a 2000ppr encoder returning 4000ppr)
// at 10Hz sample rate we can go upto 4500RPM without wrapping
// using a moving average to steady the RPM reading
float Lathe::handle_rpm_encoder(uint32_t deltams)
{
    static float ave[10];
    static int ave_cnt = 0;
    static uint32_t last = 0;
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t qediv = get_quadrature_encoder_div();
    uint32_t cnt = read_quadrature_encoder();
    uint32_t c = (cnt > last) ? cnt - last : last - cnt;

    last = cnt;

    // deal with over/underflow
    if(c > qemax / 2 ) {
        c = qemax - c + 1;
    }
    // get RPM
    float r = (c * 60 * (1000.0F / deltams)) / (ppr * qediv);

    if(ave_cnt < 10) {
        // fill the array first
        ave[ave_cnt++] = r;
    } else {
        // use moving average
        float sum = r;
        for (int i = 0; i < 10 - 1; ++i) {
            ave[i] = ave[i + 1];
            sum += ave[i];
        }
        ave[9] = r;
        r = sum / 10;
    }

    return r;
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
_ramfunc_
float Lathe::get_encoder_delta()
{
    static uint32_t last_cnt = 0;
    float delta = 0;
    uint32_t cnt = read_quadrature_encoder();
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t qediv = get_quadrature_encoder_div();
    int sign = 1;

    // handle encoder wrap around and get encoder pulses since last read
    if(cnt < last_cnt && (last_cnt - cnt) > (qemax / 2)) {
        delta = (qemax - last_cnt) + cnt + 1;
        sign = 1;
    } else if(cnt > last_cnt && (cnt - last_cnt) > (qemax / 2)) {
        delta = (qemax - cnt) + last_cnt + 1;
        sign = -1;
    } else if(cnt > last_cnt) {
        delta = cnt - last_cnt;
        sign = 1;
    } else if(cnt < last_cnt) {
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
// This maybe preferable (if possible), as it currently is when the distance is reached it stops abruptly with no deceleration
// similarly it starts abruptly with no acceleration
_ramfunc_
int Lathe::update_position()
{
    if(!running || Module::is_halted()) return -2;

    float current_position = stepper_motor->get_current_position();

    if(!std::isnan(end_pos)) {
        // G33 Znnn mode run the lead screw at the given rate (mm/rev in dpr) until distance is reached
        // check if we have travelled the required distance
        #if 0
        // FIXME an equality operation is probably risky here we need to do > or < based on direction of travel
        if(equal_within(end_pos, current_position, delta_mm)) {
            running = false;
            return -2;
        }
        #else
        // FIXME this only works for moves in the negative Z direction
        if(current_position <= end_pos) {
            running = false;
            return -2;
        }
        #endif
    }

    float delta = get_encoder_delta();

    if(delta != 0) {
        // calculate fraction of a rotation since last time and based on dpr calculate how far to move
        float mm = dpr * (delta / ppr); // calculate mm to move based on requested distance per rev
        target_position += mm; // accumulate target move
    }

    // issue steps until we hit the target move specified by the encoder/chuck rotation
    // we first check if the target is equal to current within the limits of the step increment
    if(!equal_within(target_position, current_position, delta_mm)) {
        if(target_position > current_position) {
            if(current_direction) {
                stepper_motor->set_direction(false ^ reversed);
                current_direction = false;
            }
            stepper_motor->step();
            return motor_id;
        } else if(target_position < current_position) {
            if(!current_direction) {
                stepper_motor->set_direction(true ^ reversed);
                current_direction = true;
            }
            stepper_motor->step();
            return motor_id;
        }
    }

    return -1;
}

void Lathe::on_halt(bool flg)
{
    if(flg) {
        running = false;
    }
}
