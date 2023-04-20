// Electronic Leadscrew as seen in Clough42 et al.

#include "els.h"

#include "TM1638.h"
#include "buttonbox.h"
#include "ConfigReader.h"
#include "SlowTicker.h"
#include "main.h"
#include "Lathe.h"
#include "OutputStream.h"
#include "MessageQueue.h"

#include <cmath>

#define enable_key "enable"

REGISTER_MODULE(ELS, ELS::create)

bool ELS::create(ConfigReader& cr)
{
    printf("DEBUG: configure ELS\n");
    ELS *t = new ELS();
    if(!t->configure(cr)) {
        printf("INFO: ELS not enabled\n");
        delete t;
    }
    return true;
}

ELS::ELS() : Module("ELS")
{}

bool ELS::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("els", m)) return false;

    bool enable = cr.get_bool(m, enable_key , false);
    if(!enable) {
        return false;
    }

    // register a startup function that will be called after all modules have been loaded
    // (as this module relies on the tm1638, buttonbox and Lathe modules having been loaded)
    register_startup(std::bind(&ELS::after_load, this));

    return true;
}

void ELS::after_load()
{
    Module *v= Module::lookup("Lathe");
    if(v == nullptr) {
        printf("ERROR: ELS: Lathe is not available\n");
        return;
    }

    lathe= static_cast<Lathe*>(v);

    // get display if available
    v= Module::lookup("tm1638");
    if(v != nullptr) {
        tm=  static_cast<TM1638*>(v);
        tm->lock();
        tm->displayBegin();
        tm->reset();
        tm->unlock();

        printf("DEBUG: ELS: display started\n");
    }else{
        printf("ERROR: ELS: display is not available\n");
        return;
    }

    // start up timers
    SlowTicker::getInstance()->attach(10, std::bind(&ELS::check_buttons, this));
    SlowTicker::getInstance()->attach(1, std::bind(&ELS::update_rpm, this));
}

void ELS::update_rpm()
{
    if(tm == nullptr) return;

    // update display once per second
    if(tm->lock()) {
        float rpm= lathe->get_rpm();

        // display if running and what mode it is in
        tm->displayIntNum((int)roundf(rpm), false, TMAlignTextRight);
        if(lathe->is_running()) {
            tm->setLED(std::isnan(lathe->get_distance())?0:1, 1);
        }else{
            tm->setLED(std::isnan(lathe->get_distance())?0:1, 0);
        }
        tm->unlock();
    }
}

// called in slow timer every 100ms to scan buttons
void ELS::check_buttons()
{
    static uint8_t last_buttons= 0;

    if(tm == nullptr) return;

    if(tm->lock()) {
        buttons = tm->readButtons();
        tm->unlock();
    }

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

    static OutputStream os; // NULL output stream, but we need to keep some state between calls

    if((buttons & 0x01) && !(last_buttons & 0x01)) {
        // button 1 pressed - Stop current operation
        os.set_stop_request(true);
    }

    if((buttons & 0x02) && !(last_buttons & 0x02)) {
        // button 2 pressed, start operation G33 K1
        send_message_queue("G33 K1", &os, false);
    }

    last_buttons= buttons;
}




