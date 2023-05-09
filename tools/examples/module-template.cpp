#include "TEMPLATE.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Dispatcher.h"
#include "GCode.h"

#define template_enable_key "enable"

REGISTER_MODULE(TEMPLATE, TEMPLATE::create)

bool TEMPLATE::create(ConfigReader& cr)
{
    printf("DEBUG: configure TEMPLATE\n");
    TEMPLATE *t = new TEMPLATE();
    if(!t->configure(cr)) {
        printf("INFO: No TEMPLATE enabled\n");
        delete t;
    }
    return true;
}

TEMPLATE::TEMPLATE() : Module("TEMPLATE")
{
}

bool TEMPLATE::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("template", m)) return false;

    bool template_enable = cr.get_bool(m,  template_enable_key , false);
    if(!template_enable) {
        return false;
    }

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 1234, std::bind(&TEMPLATE::handle_gcode, this, _1, _2));

/* sample of creating a user defined button and scanning it
    if(this->template_button.from_string(cr.get_string(m,  template_button_pin_key , "nc"))) {
        template_button.as_input();
    }

    if(!this->template_button.connected()) {
        printf("ERROR: configure-template: No, or illegal, button defined\n");
        return false;
    }

    SlowTicker::getInstance()->attach(20, std::bind(&TEMPLATE::button_tick, this));
*/

    return true;
}

bool TEMPLATE::handle_gcode(GCode& gcode, OutputStream& os)
{
    // get "G" value
    int code = gcode.get_code();

    // example of handling an error when a gcode must provide an argument
    if(gcode.has_no_args()) {
        gcode.set_error("No arguments provided");
        return true;
    }

    if(code == 1234) { // not neeed if only handling one gcode
        if (gcode.has_arg('X')) {
            float arg = gcode.get_arg('X');
            // .....
        }
        return true;
    }

    // if not handled
    return false;
}


// Check the state of the button and act accordingly
// Note this is the system timer so don't do anything slow in here
/*
void TEMPLATE::button_tick()
{
}
*/
