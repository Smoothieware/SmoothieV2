#include "els.h"

#include "TM1638.h"
#include "buttonbox.h"
#include "ConfigReader.h"
#include "SlowTicker.h"
#include "main.h"
#include "Lathe.h"

#include <cmath>

#define enable_key "enable"

// define a map of button names and but position
// NOTE these buttons need to be defined in button box config as well
static std::map<std::string, uint8_t> button_names = {
    {"els-b1", 0x01},
    {"els-b2", 0x02},
    {"els-b3", 0x04},
    {"els-b4", 0x08},
    {"els-b5", 0x10},
    {"els-b6", 0x20},
    {"els-b7", 0x40},
    {"els-b8", 0x80},
};

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
    // (as this module relies on the tm1638 and buttonbox and Lathe modules having been loaded)
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
        TM1638 *tm=  static_cast<TM1638*>(v);
        display= tm;
        tm->lock();
        tm->displayBegin();
        tm->reset();
        tm->unlock();

        printf("DEBUG: ELS: display started\n");
    }else{
        printf("ERROR: ELS: display is not available\n");
        return;
    }


    // button box is used to define button functions
    v= Module::lookup("buttonbox");
    if(v != nullptr) {
        ButtonBox *bb=  static_cast<ButtonBox*>(v);
         // assign a callback for each of the buttons
        // we need to match button names here with what was defined in button box config
        using std::placeholders::_1;
        for(auto i : button_names) {
            if(!bb->set_cb_fnc(i.first.c_str(), std::bind(&ELS::check_button, this, _1))){
                printf("WARNING: ELS: button %s was not defined in button box\n", i.first.c_str());
            }
        }

    }else{
        printf("ERROR: ELS: button box is not available\n");
        return;
    }

    SlowTicker::getInstance()->attach(250, std::bind(&ELS::check_buttons, this));
}

void ELS::update_rpm()
{
    if(display == nullptr) return;

    // update display once per second
    TM1638 *tm=  static_cast<TM1638*>(display);
    if(tm->lock()) {
        float rpm= lathe->get_rpm();

        tm->displayIntNum((int)roundf(rpm), false, TMAlignTextRight);
        // if(running) {
        //     tm->setLED(std::isnan(distance)?0:1, 1);
        // }else{
        //     tm->setLED(std::isnan(distance)?0:1, 0);
        // }
        tm->unlock();
    }
}

// called in slow timer every 100ms to scan buttons
void ELS::check_buttons()
{
    TM1638 *tm=  static_cast<TM1638*>(display);
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
}

bool ELS::check_button(const char *name)
{
    auto bm= button_names.find(name);
    if(bm != button_names.end()) {
        return (buttons & bm->second) != 0;
    }

    return false;
}



