#include "Lathe.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Dispatcher.h"
#include "GCode.h"
#include "FastTicker.h"
#include "SlowTicker.h"
#include "QuadEncoder.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "main.h"
#include "OutputStream.h"

#include <cmath>

#define lathe_enable_key "enable"
#define lathe_ppr_key "encoder_ppr"

#define UPDATE_HZ 1000
#define RPM_UPDATE_HZ 2

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
}

bool Lathe::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("lathe", m)) return false;

    bool lathe_enable = cr.get_bool(m,  lathe_enable_key , false);
    if(!lathe_enable) {
        return false;
    }

    if(!setup_quadrature_encoder()) {
        printf("ERROR: configure-lathe: unable to setup quadrature encoder\n");
        return false;
    }

    // pulses per rotation (takes into consideration any gearing) ppr= encoder resolution * gear ratio
    ppr = cr.get_float(m, lathe_ppr_key, 1000);
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

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 33, std::bind(&Lathe::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler("rpm", std::bind( &Lathe::rpm_cmd, this, _1, _2) );

    FastTicker::getInstance()->attach(UPDATE_HZ, std::bind(&Lathe::update_position, this));
    SlowTicker::getInstance()->attach(RPM_UPDATE_HZ, std::bind(&Lathe::handle_rpm, this));

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
            running = true;
            while(!os.get_stop_request() && !Module::is_halted()) {
                safe_sleep(100);
                // update DROs occasionally
                Robot::getInstance()->reset_position_from_current_actuator_position();
                //printf("%f %ld\n", target_position, read_quadrature_encoder());
            }
            running = false;
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

// called every 1/2 second to calculate current RPM
void Lathe::handle_rpm()
{
    static uint32_t last= 0;
    uint32_t qemax = get_quadrature_encoder_max_count();
    uint32_t cnt = abs(read_quadrature_encoder())>>2;
    uint32_t c = (cnt > last) ? cnt - last : last - cnt;
    last = cnt;

    // deal with over/underflow
    if(c > qemax/2 ) {
        c= qemax - c + 1;
    }

    rpm = (c * 60 * RPM_UPDATE_HZ) / ppr;
}

// given move in spindle, calculate where the controlled axis should be
float Lathe::calculate_position(int32_t cnt)
{
    // TODO calculate position given counts per rotation
    // TODO fixed 100 cpr needs to be set in config though
    float mm_per_rotation = 1.0F;
    return cnt / 100.0F * mm_per_rotation;
}

int32_t Lathe::get_encoder_delta()
{
    float delta;
    int32_t cnt = read_quadrature_encoder()>>2;
    int32_t qemax = get_quadrature_encoder_max_count();

    // handle encoder wrap around and get encoder pulses since last read
    if(cnt < last_cnt && (last_cnt - cnt) > (qemax / 2)) {
        delta = (qemax - last_cnt) + cnt + 1;
    } else if(cnt > last_cnt && (cnt - last_cnt) > (qemax / 2)) {
        delta = (qemax - cnt) + 1;
    } else {
        delta = cnt - last_cnt;
    }
    last_cnt = cnt;

    return delta;
}

void Lathe::update_position()
{
    if(!running || Module::is_halted()) return;

    if(std::isnan(distance)) {
        // just run the lead screw at the given rate (mm/rev in dpr) until told to stop

        int32_t delta= get_encoder_delta();

        if(delta != 0) {
            // calculate fraction of a rotation since last time and based on dpr calculate how far to move
            float mm = dpr * (delta / ppr); // calculate mm to move based on distance per rev
            target_position += mm; // accumulate target move
        }

        // hopefully this runs faster then the spindle rotates so it will keep up
        // even though we only issue one step per pass (currently 5000 steps/sec max)
        float current_position = stepper_motor->get_current_position();
        if(target_position > current_position) {
            stepper_motor->manual_step(false);

        } else if(target_position < current_position) {
            stepper_motor->manual_step(true);
        }
        return;
    }

    // TODO this is not G33 mode, currently G33 just turns on the synchronized spindle mode
    // which will travel for the given distance before suddenly stopping

    // get current spindle position
    int32_t cnt = read_quadrature_encoder();
    wanted_pos = calculate_position(cnt);

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

void Lathe::on_halt(bool flg)
{
    if(flg) {
        running= false;
    }
}
