// Electronic Leadscrew as seen in Clough42 et al.

#include "dro.h"

#include "max7219.h"
#include "buttonbox.h"
#include "ConfigReader.h"
#include "SlowTicker.h"
#include "main.h"
#include "OutputStream.h"
#include "MessageQueue.h"
#include "Robot.h"
#include "StepperMotor.h"

#include <cmath>
#include <string>
#include <iostream>
#include <ctype.h>

#define enable_key "enable"
#define cs_pin_key "cs_pin"
#define id_key "id"
#define poll_freq_key "poll_frequency_hz"

REGISTER_MODULE(DRO, DRO::create)

bool DRO::create(ConfigReader& cr)
{
    printf("DEBUG: configure DRO\n");
    DRO *t = new DRO();
    if(!t->configure(cr)) {
        printf("INFO: DRO not enabled\n");
        delete t;
    }
    return true;
}

DRO::DRO() : Module("DRO")
{}

bool DRO::configure(ConfigReader& cr)
{
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("dro", ssmap)) {
        printf("INFO: configure-dro: no dro section found\n");
        return false;
    }

    auto s = ssmap.find("common");
    if(s != ssmap.end()) {
        auto& mm = s->second; // map of common config settings
        if(!cr.get_bool(mm, enable_key, true)) return false; // exit if not enabled
        poll_freq = cr.get_int(mm, poll_freq_key, 10);
        printf("INFO: configure-dro: poll freq set to %ld hz\n", poll_freq);
    }

    // get the CS for each axis
    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach axis, name needs to be x,y,z,a,b,c
        std::string name = i.first;
        if(name == "common") continue;

        auto& m = i.second;
        if(!cr.get_bool(m, enable_key, true)) continue; // skip if not enabled

        // check it is a valid axis designation
        if(name.size() != 1 || name.find_first_of("xyzabc") == name.npos) {
            printf("ERROR: configure-dro: axis %s is not one of xyzabc\n", name.c_str());
            continue;
        }

        // get axis id or cs pin (id takes precedence)
        std::string p = cr.get_string(m, id_key, "");
        if(p.empty()) {
            // try cs_pin
            p = cr.get_string(m, cs_pin_key, "nc");
        }
        axis_cs[name] = p;
        ++cnt;
    }

    printf("INFO: configure-dro: %d axis loaded\n", cnt);

    if(cnt > 0) {
        // register a startup function that will be called after all modules have been loaded
        // (as this module relies on the max7219 module having been loaded)
        register_startup(std::bind(&DRO::after_load, this));

        // start up timer
        SlowTicker::getInstance()->attach(poll_freq, std::bind(&DRO::update_display, this));
    }

    return true;
}

static bool is_number(const char *s) {
    for (uint i = 0; i < strlen(s); ++i) {
        if(!isdigit(s[i])) return false;
    }
    return strlen(s) > 0;
}

void DRO::after_load()
{
    printf("DEBUG: DRO post config running...\n");

    // get display if available
    Module *v= Module::lookup("max7219");
    if(v != nullptr) {
        display=  static_cast<MAX7219*>(v);
    }else{
        printf("ERROR: DRO MAX7219 display is not available\n");
        return;
    }

    // create instances for each axis with its oown cs or id depending if cascaded or not
    for(auto& i : axis_cs) {
        std::string a = i.first;
        const char *pin= i.second.c_str();
        int id= -1;
        if(display->is_cascaded()) {
            // specify the id of the module for each axis if cascaded
            if(is_number(pin)) {
                id = atoi(pin);
            } else {
                printf("ERROR: DRO display axis %s id is not a number: %s\n", a.c_str(), pin);
                continue;
            }

        } else {
            // specify the cs pin of the module for each axis if not cascaded
            id = display->add_instance(pin);
        }

        if(id >= 0) {
            char c = a[0];
            int n = c - 'x';
            if(n < 0) n = c - 'a';
            axis_map[n] = id;
            //display->lock();
            display->clear(id);
            //display->unlock();
            if(display->is_cascaded()) {
                printf("DEBUG: DRO added axis %s (%d) with id %d\n", a.c_str(), n, id);
            } else {
                printf("DEBUG: DRO added axis %s (%d) with cs pin %s\n", a.c_str(), n, pin);
            }
        } else {
            printf("ERROR: DRO display axis %s CS pin is not valid: %s\n", a.c_str(), pin);
        }
    }
    axis_cs.clear(); // no longer needed

    display->init();
    printf("DEBUG: DRO MAX7219 display started\n");

    started= true;
}

void DRO::update_display()
{
    if(!started) return;

    float mpos[3];
    Robot::getInstance()->get_current_machine_position(mpos);
    // convert to work space position
    Robot::wcs_t pos = Robot::getInstance()->mcs2wcs(mpos);
    mpos[0] = Robot::getInstance()->from_millimeters(std::get<X_AXIS>(pos));
    mpos[1] = Robot::getInstance()->from_millimeters(std::get<Y_AXIS>(pos));
    mpos[2] = Robot::getInstance()->from_millimeters(std::get<Z_AXIS>(pos));

    for(auto& i : axis_map) {
        int a = i.first;
        int id = i.second;
        if(a >= X_AXIS && a <= Z_AXIS) {
            float p = mpos[a];
            display->display_float3(id, p);
        }
#if MAX_ROBOT_ACTUATORS > 3
        else {
            // deal with the ABC axis (E will be A)
            if(a >= A_AXIS && a < Robot::getInstance()->get_number_registered_motors()) {
                // current actuator position
                float p = Robot::getInstance()->actuators[a]->get_current_position();
                display->display_float3(id, p);
            }
        }
#endif
    }
}
