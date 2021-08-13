#include "TEMPLATE.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"

#define template_enable_key "enable"

REGISTER_MODULE(TEMPLATE, TEMPLATE::create)

bool TEMPLATE::create(ConfigReader& cr)
{
    printf("DEBUG: configure TEMPLATE\n");
    TEMPLATE *template = new TEMPLATE();
    if(!template->configure(cr)) {
        printf("INFO: No TEMPLATE enabled\n");
        delete template;
        template = nullptr;
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

// Check the state of the button and act accordingly
// Note this is the system timer so don't do anything slow in here
/*
void TEMPLATE::button_tick()
{
}
*/
