/*
    Handles a filament detector that has an optical encoder wheel, that generates pulses as the filament
    moves through it.
    It also supports a "bulge" detector that triggers if the filament has a bulge in it.

    It can also support a simple filament detector that is just a microswitch that triggers when the filament is not in it.
      In this case use the detector pin only, and do not define the encoder pulse pin.
*/

#include "FilamentDetector.h"
#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Dispatcher.h"
#include "GCode.h"
#include "OutputStream.h"

#include <cmath>

#define enable_key "enable"
#define encoder_pin_key "encoder_pin"
#define bulge_pin_key "bulge_pin"
#define detector_pin_key "detector_pin"
#define seconds_per_check_key "seconds_per_check"
#define pulses_per_mm_key "pulses_per_mm"
#define leave_heaters_on_key "leave_heaters_on"

REGISTER_MODULE(FilamentDetector, FilamentDetector::create)

FilamentDetector::FilamentDetector() : Module("FilamentDetector")
{
    filament_out_alarm= false;
    bulge_detected= false;
    active= true;
    e_last_moved= NAN;
    was_retract= false;
    triggered= false;
}

FilamentDetector::~FilamentDetector()
{
}

bool FilamentDetector::create(ConfigReader& cr)
{
    printf("DEBUG: configure FilamentDetector\n");
    FilamentDetector *fd = new FilamentDetector();
    if(!fd->configure(cr)) {
        printf("INFO: FilamentDetector not enabled\n");
        delete fd;
    }
    return true;
}

bool FilamentDetector::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("filament detector", m)) return false;

    // if the module is disabled -> do nothing
    if(!cr.get_bool(m, enable_key, false)) {
        // as this module is not needed free up the resource
        return false;
    }

    // encoder pin has to be an interrupt enabled pin
    encoder_pin= new Pin(cr.get_string(m, encoder_pin_key, "nc" ));
    if(encoder_pin->connected()) {
        if(!encoder_pin->as_interrupt(std::bind(&FilamentDetector::on_pin_rise, this))) {
            printf("ERROR: filament-detector: illegal interrupt pin\n");
            delete encoder_pin;
            encoder_pin= nullptr;
        }

    } else{
        delete encoder_pin;
        encoder_pin= nullptr;
    }

    // optional bulge detector
    bulge_pin= new Pin(cr.get_string(m, bulge_pin_key, "nc" ));
    if(bulge_pin->connected()) {
        // input pin polling
        SlowTicker::getInstance()->attach(100, std::bind(&FilamentDetector::button_tick, this));

    }else{
        delete bulge_pin;
        bulge_pin= nullptr;

        // or detector pin
        detector_pin= new Pin(cr.get_string(m, detector_pin_key, "nc" ));
        if(detector_pin->connected()) {
            // input pin polling
            SlowTicker::getInstance()->attach(100, std::bind(&FilamentDetector::button_tick, this));
        }else{
            delete detector_pin;
            detector_pin= nullptr;
        }
    }

    //Valid configurations contain an encoder pin, a bulge pin or both.
    //free the module if not a valid configuration
    if(encoder_pin == nullptr && bulge_pin == nullptr && detector_pin == nullptr) {
        printf("ERROR: filament-detector: No detector pins defined\n");
        return false;
    }

    // how many seconds between checks, must be long enough for several pulses to be detected, but not too long
    seconds_per_check= cr.get_float(m, seconds_per_check_key, 2);

    // the number of pulses per mm of filament moving through the detector, can be fractional
    pulses_per_mm= cr.get_float(m, pulses_per_mm_key, 1);

    // leave heaters on when it suspends
    leave_heaters_on= cr.get_bool(m, leave_heaters_on_key , false);

    // register mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 404, std::bind(&FilamentDetector::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 405, std::bind(&FilamentDetector::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 406, std::bind(&FilamentDetector::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 407, std::bind(&FilamentDetector::handle_mcode, this, _1, _2));

    // in command ctx is called
    want_command_ctx= true;

    if(encoder_pin != nullptr) {
        // need to check pulses every now and then
        SlowTicker::getInstance()->attach(1, std::bind(&FilamentDetector::on_second_tick, this));
    }

    return true;
}

float FilamentDetector::get_emove()
{
    auto mv = Module::lookup_group("extruder");
    if(mv.size() > 0) {
        for(auto m : mv) {
            float data;
            if(m->request("get_current_position", &data)) {
                return data;
                // only one can be active the rest will return false
            }
        }
    }

    return NAN;
}

bool FilamentDetector::handle_mcode(GCode& gcode, OutputStream& os)
{
    int code = gcode.get_code();

    if (code == 404) { // temporarily set filament detector parameters S seconds per check, P pulses per mm, H leave heaters on
        if(gcode.has_arg('S')){
            seconds_per_check= gcode.get_arg('S');
            seconds_passed= 0;
        }
        if(gcode.has_arg('P')){
            pulses_per_mm= gcode.get_arg('P');
        }
        if(gcode.has_arg('H')){
            leave_heaters_on= gcode.get_arg('H') >= 1;
        }

        os.printf("// pulses per mm: %f, seconds per check: %d, heaters: %s\n", pulses_per_mm, seconds_per_check, leave_heaters_on?"on":"off");
        return true;

    } else if (code == 405) { // disable filament detector
        this->pulses= 0;
        active= false;
        e_last_moved= get_emove();
        return true;

    }else if (code == 406) { // enable filament detector
        this->pulses= 0;
        e_last_moved=  get_emove();
        active= true;
        return true;

    }else if (code == 407) { // display filament detector pulses and status
        float e_moved= get_emove();
        if(!std::isnan(e_moved)) {
            float delta= e_moved - e_last_moved;
            os.printf("Extruder moved: %f mm\n", delta);
        }

        os.printf("Encoder pulses: %u\n", pulses.load());
        if(triggered) os.printf("Filament detector has been triggered\n");
        if(is_suspended()) os.printf("system is suspended\n");
        os.printf("Filament detector is %s\n", active?"enabled":"disabled");
        os.printf("Leave heaters on is %s\n", leave_heaters_on?"true":"false");
        return true;
    }

    return false;
}

bool FilamentDetector::is_suspended() const
{
    auto m= Module::lookup("player");
    if(m != nullptr) {
        bool r= false;
        bool ok = m->request("is_suspended", &r);
        if(ok && r) {
            return true;
        }
    }

    return false;
}

// This is called periodically to allow commands to be issued in the command thread context
// but only when want_command_ctx is set to true
void FilamentDetector::in_command_ctx(bool idle)
{
    if(triggered) {
        // if we have been triggered we check to see if we are still suspended,
        // and clear the state if we are not suspended anymore
        if(!is_suspended()) {
            triggered= false;
            pulses= 0;
            e_last_moved= NAN;
        }
        return;
    }

    if (active && filament_out_alarm) {
        filament_out_alarm = false;
        if(bulge_detected){
            print_to_all_consoles("// Filament Detector has detected a bulge in the filament\n");
            bulge_detected= false;
        }else{
            print_to_all_consoles("// Filament Detector has detected a filament jam/break\n");
        }

        if(!is_suspended()) {
            // fire suspend command
            OutputStream os; // null output stream
            THEDISPATCHER->dispatch(os, 'M', 600, leave_heaters_on?1:0, 0);
            triggered= true;
        }
    }
}

void FilamentDetector::on_second_tick()
{
    if(++seconds_passed >= seconds_per_check) {
        seconds_passed= 0;
        check_encoder();
    }
}

// encoder pin interrupt
void FilamentDetector::on_pin_rise()
{
    this->pulses++;
}

void FilamentDetector::check_encoder()
{
    if(!active) return;  // not enabled
    if(is_suspended()) return; // already suspended

    uint32_t pulse_cnt= this->pulses.exchange(0); // atomic load and reset

    // get number of E steps taken and make sure we have seen enough pulses to cover that
    float e_moved= get_emove();
    if(std::isnan(e_last_moved)) {
        e_last_moved= e_moved;
        return;
    }

    float delta= e_moved - e_last_moved;
    e_last_moved= e_moved;
    if(delta == 0) {
        // no movemement
        return;

    }else if(delta < 0) {
        // if E is reset then we will be negative, this usually happens after a retract
        // we ignore retracts for the purposes of jam detection
        was_retract= true;
        return;

    }else if(delta > 0 && was_retract) {
        // ignore first extrude after a retract
        was_retract= false;
        return;
    }

    // figure out how many pulses need to have happened to cover that e move
    uint32_t needed_pulses= floorf(delta*pulses_per_mm);
    // NOTE if needed_pulses is 0 then extruder did not move since last check, or not enough to register
    if(needed_pulses == 0) return;

    if(pulse_cnt == 0) {
        // we got no pulses and E moved since last time so fire off alarm
        this->filament_out_alarm= true;
    }
}

void FilamentDetector::button_tick()
{
    if(bulge_pin == nullptr && detector_pin == nullptr) return;

    if(!active || is_suspended()) return;

    if((bulge_pin != nullptr && bulge_pin->get()) ||
       (detector_pin != nullptr && detector_pin->get())) {
        // we got a trigger from the bulge detector
        filament_out_alarm= true;
        if(bulge_pin != nullptr) bulge_detected= true;
    }

    return;
}
