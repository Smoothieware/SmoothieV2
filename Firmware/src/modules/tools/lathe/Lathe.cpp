#include "Lathe.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Dispatcher.h"
#include "GCode.h"
#include "FastTicker.h"
#include "QuadEncoder.h"
#include "Robot.h"
#include "StepperMotor.h"

#define lathe_enable_key "enable"

REGISTER_MODULE(Lathe, Lathe::create)

bool Lathe::create(ConfigReader& cr)
{
    printf("DEBUG: configure Lathe\n");
    Lathe *t = new Lathe();
    if(!t->configure(cr)) {
        printf("INFO: No Lathe enabled\n");
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

    // TODO needs to be configurable
    stepper_motor = Robot::getInstance()->actuators[A_AXIS];
    motor_id = stepper_motor->get_motor_id();
    if(motor_id < A_AXIS || motor_id == 255) {
        // error registering, maybe too many
        printf("ERROR: configure-lathe: unable to get stepper motor\n");
        return false;
    }

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    //Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 1234, std::bind(&Lathe::handle_gcode, this, _1, _2));

    FastTicker::getInstance()->attach(1000, std::bind(&Lathe::update_position, this));

    return true;
}

#if 0
bool Lathe::handle_gcode(GCode& gcode, OutputStream& os)
{
    // get "G" value
    int code = gcode.get_code();

    // example of handling an error when a gcode must provide an argument
    if(gcode.has_no_args()) {
        gcode.set_error("No arguments provided");
        return true;
    }

    if(code == 1234) { // not needed if only handling one gcode
        if (gcode.has_arg('X')) {
            float arg = gcode.get_arg('X');
            // .....
        }
        return true;
    }

    // if not handled
    return false;
}
#endif

// given move in spindle, calculate where the controlled axis should be
float Lathe::calculate_position(int32_t cnt)
{
    // TODO calculate position given counts per rotation
    // TODO fixed 2000 cpr needs to be set in config though
    float mm_per_rotation= 10.0F;
    return cnt/2000.0F * mm_per_rotation;
}

void Lathe::update_position()
{
    if(Module::is_halted()) return;

    // get current spindle position
    int32_t cnt = read_quadrature_encoder();
    wanted_pos = calculate_position(cnt);

    // compare with current position and issue step if needed
    float current_position= stepper_motor->get_current_position();
    if(wanted_pos > current_position) {
        stepper_motor->manual_step(true);

    } else if(wanted_pos < current_position) {
        stepper_motor->manual_step(false);
    }

}
