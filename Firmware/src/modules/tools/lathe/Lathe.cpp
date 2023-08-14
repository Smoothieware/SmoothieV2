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
#include "benchmark_timer.h"

#include "FreeRTOS.h"
#include "task.h"


#include <cmath>

#define enable_key "enable"
#define ppr_key "encoder_ppr"
#define index_pin_key "index_pin"

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

    if(!setup_quadrature_encoder()) {
        printf("ERROR: configure-lathe: unable to setup quadrature encoder\n");
        return false;
    }

    // use index pin if we define one
    index_pin =  new Pin(cr.get_string(m, index_pin_key, "nc"));
    if(index_pin->connected()) {
        index_pin->as_interrupt(std::bind(&Lathe::handle_index_irq, this), Pin::RISING);
        if(!index_pin->connected()) {
            printf("ERROR: configure-lathe: Cannot set index pin to interrupt %s\n", index_pin->to_string().c_str());
            delete index_pin;
            index_pin = nullptr;
        }
        printf("INFO: configure-lathe: using index pin: %s\n", index_pin->to_string().c_str());

    } else {
        delete index_pin;
        index_pin = nullptr;
        printf("INFO: configure-lathe: no index pin\n");
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

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    benchmark_timer_init();

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
            end_pos = stepper_motor->get_current_position() + distance;

            if(distance < 0) {
                reversed = true;
                distance = -distance;
            } else {
                reversed = false;
            }

            // if we have an index_pin then we wait to start by synchronizing to it
            if(index_pin != nullptr) {
                uint32_t curindex = index_pulse;
                while(curindex == index_pulse) {
                    // wait for index pulse to be hit
                    // TODO may need to do safe_sleep here but that may take too long
                    if(Module::is_halted() || rpm == 0) return true;
                }
            }

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

            running = false;

            // reset the position based on current actuator position
            Robot::getInstance()->reset_position_from_current_actuator_position();

        } else if(gcode.has_arg('X') || gcode.has_arg('Y')) {
            gcode.set_error("Only (Lathe) Z axis currently supported");

        } else {
            // no Z arg means manual mode where the half nut must be engaged and disengaged, control Y will stop it
            // K sets the mm per revolution
            end_pos = NAN;
            target_position = stepper_motor->get_current_position();
            if(!stepper_motor->is_enabled()) stepper_motor->enable(true);
            current_direction = stepper_motor->get_direction();

            // have stepticker call us
            StepTicker::getInstance()->callback_fnc = std::bind(&Lathe::update_position, this);
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

            StepTicker::getInstance()->callback_fnc = nullptr;

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

void Lathe::handle_index_irq()
{
    // count index pulses
    ++index_pulse;
}

// called every 100 ms to calculate current RPM
void Lathe::handle_rpm()
{
    static uint32_t last_index_pulse = 0;
    static uint32_t lasttime = 0;

    if(lasttime == 0 || benchmark_timer_wrapped(lasttime)) {
        lasttime = benchmark_timer_start();
        return;
    }

    // get elapsed time since last call, more accurate than relying on 100ms timer
    uint32_t deltams = benchmark_timer_as_ms(benchmark_timer_elapsed(lasttime));

    if(index_pin != nullptr) {
        // use the index pin to calculate RPM
        // sample about every second to increase pulse count captured
        if(deltams >= 1000) {
            lasttime = benchmark_timer_start();
            if(last_index_pulse > index_pulse) {
                // we wrapped so skip this one
                last_index_pulse = index_pulse;
                return;
            }

            uint32_t d = index_pulse - last_index_pulse;
            last_index_pulse = index_pulse;
            rpm = (d * 60 * (1000.0F / deltams));
        }

    } else {
        // use encoder to calculate RPM
        lasttime = benchmark_timer_start();
        handle_rpm_encoder(deltams);
    }
}

// calculate RPM from Encoder
// Note at .5 secs sample rate we would wrap the counter at 960RPM and get a false reading (with a 2000ppr encoder returning 4000ppr)
// at 10Hz sample rate we can go upto 4500RPM without wrapping
// using a moving average to steady the RPM reading
void Lathe::handle_rpm_encoder(uint32_t deltams)
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
        rpm = r;
    } else {
        // use moving average
        float sum = r;
        for (int i = 0; i < 10 - 1; ++i) {
            ave[i] = ave[i + 1];
            sum += ave[i];
        }
        ave[9] = r;
        rpm = sum / 10;
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
_ramfunc_
int Lathe::update_position()
{
    if(!running || Module::is_halted()) return -1;

    float current_position = stepper_motor->get_current_position();

    if(!std::isnan(end_pos)) {
        // G33 Znnn mode run the lead screw at the given rate (mm/rev in dpr) until distance is reached
        // check if we have travelled the required distance
        // FIXME an equality operation is probably risky here we need to do > or < based on direction of travel
        if(equal_within(end_pos, current_position, delta_mm)) {
            running = false;
            return -1;
        }
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
