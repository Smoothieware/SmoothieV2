#include "drillingcycles.h"
#include "ConfigReader.h"
#include "GCode.h"
#include "OutputStream.h"
#include "Dispatcher.h"
#include "Robot.h"
#include "Conveyor.h"

#include <math.h>

// retract modes
#define RETRACT_TO_Z 0
#define RETRACT_TO_R 1

// dwell units
#define DWELL_UNITS_S 0 // seconds
#define DWELL_UNITS_P 1 // millis

// config names
#define drillingcycles_key "drillingcycles"
#define enable_key "enable"
#define dwell_units_key "dwell_units"

REGISTER_MODULE(Drillingcycles, Drillingcycles::create)

bool Drillingcycles::create(ConfigReader& cr)
{
    printf("DEBUG: configure Drillingcycles\n");
    Drillingcycles *dc = new Drillingcycles();
    if(!dc->configure(cr)) {
        printf("INFO: Drillingcycles not enabled\n");
        delete dc;
    }
    return true;
}

Drillingcycles::Drillingcycles() : Module("Drillingcycles")
{
}

bool Drillingcycles::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("drillingcycles", m)) {
        printf("INFO: config-drillingcycles: no drillingcycles section found, presume disabled\n");
        return false;
    }

    // if the module is disabled -> do nothing
    if(!cr.get_bool(m, enable_key, false)) {
        return false;
    }

    // take the dwell units configured by user, or select S (seconds) by default
    std::string s= cr.get_string(m, dwell_units_key, "S");
    this->dwell_units= (s == "P") ? DWELL_UNITS_P : DWELL_UNITS_S;

    // reset values
    this->cycle_started = false;
    this->retract_type  = RETRACT_TO_Z;

    this->initial_z = 0;
    this->r_plane   = 0;

    this->reset_sticky();

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 80, std::bind(&Drillingcycles::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 81, std::bind(&Drillingcycles::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 82, std::bind(&Drillingcycles::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 83, std::bind(&Drillingcycles::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 98, std::bind(&Drillingcycles::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 99, std::bind(&Drillingcycles::handle_gcode, this, _1, _2));

    return true;
}

/*
The canned cycles have been implemented as described on this page :
http://www.tormach.com/g81_g89_backgroung.html

/!\ This code expects a clean gcode, no fail safe at this time.

Implemented     : G80-83, G98, G99
Absolute mode   : yes
Relative mode   : no
Incremental (L) : no
*/

/* reset all sticky values, called before each cycle */
void Drillingcycles::reset_sticky()
{
    this->sticky_z = 0; // Z depth
    this->sticky_r = 0; // R plane
    this->sticky_f = 0; // feedrate
    this->sticky_q = 0; // peck drilling increment
    this->sticky_p = 0; // dwell in seconds
}

/* update all sticky values, called before each hole */
void Drillingcycles::update_sticky(GCode& gcode)
{
    if (gcode.has_arg('Z')) this->sticky_z = gcode.get_arg('Z');
    if (gcode.has_arg('R')) this->sticky_r = gcode.get_arg('R');
    if (gcode.has_arg('F')) this->sticky_f = gcode.get_arg('F');
    if (gcode.has_arg('Q')) this->sticky_q = gcode.get_arg('Q');
    if (gcode.has_arg('P')) this->sticky_p = gcode.get_arg('P');

    // set retract plane
    if (this->retract_type == RETRACT_TO_Z){
        this->r_plane = this->initial_z;
    }else{
        this->r_plane = this->sticky_r;
    }
}

/* G83: peck drilling */
void Drillingcycles::peck_hole()
{
    // start values
    float depth  = this->sticky_r - this->sticky_z; // travel depth
    float cycles = depth / this->sticky_q;          // cycles count
    float rest   = fmodf(depth, this->sticky_q);     // final pass
    float z_pos  = this->sticky_r;                  // current z position
    OutputStream nullos;

    // for each cycle
    for (int i = 1; i < cycles; i++) {
        // decrement depth
        z_pos -= this->sticky_q;
        // feed down to depth at feedrate (F and Z)
        THEDISPATCHER->dispatch(nullos, 'G', 1, 'Z', z_pos, 'F', this->sticky_f, 0);
        // rapids to retract position (R)
        THEDISPATCHER->dispatch(nullos, 'G', 0, 'Z', this->sticky_r, 0);
    }

    // final depth not reached
    if (rest > 0) {
        // feed down to final depth at feedrate (F and Z)
        THEDISPATCHER->dispatch(nullos, 'G', 1, 'Z', this->sticky_z, 'F', this->sticky_f, 0);
    }
}

void Drillingcycles::make_hole(GCode& gcode)
{
    float x= NAN;
    float y= NAN;
    if (gcode.has_arg('X')) x= gcode.get_arg('X');
    if (gcode.has_arg('Y')) y= gcode.get_arg('Y');

    OutputStream nullos;

    // rapids to X/Y
    if(!isnan(x) && !isnan(y)) {
        THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', x, 'Y', y, 0);
    } else if(!isnan(x)) {
        THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', x, 0);
    } else if(!isnan(y)) {
        THEDISPATCHER->dispatch(nullos, 'G', 0, 'Y', y, 0);
    }

    // rapids to retract position (R)
    THEDISPATCHER->dispatch(nullos, 'G', 0, 'Z', this->sticky_r, 0);

    // if peck drilling
    if (this->sticky_q > 0){
        this->peck_hole();

    }else {
        // feed down to depth at feedrate (F and Z)
        THEDISPATCHER->dispatch(nullos, 'G', 1, 'Z', this->sticky_z, 'F', this->sticky_f, 0);
    }

    // if dwell, wait for x seconds
    if (this->sticky_p > 0) {
        if (this->dwell_units == DWELL_UNITS_S) {
            // dwell in seconds
            THEDISPATCHER->dispatch(nullos, 'G', 4, 'S', this->sticky_p, 0);

        } else {
            // dwell in milliseconds
            float d= this->sticky_p;
            if(Dispatcher::getInstance()->is_grbl_mode()) {
                // in grbl mode (and linuxcnc) P is decimal seconds
                d *= 1000.0F;
            }
            THEDISPATCHER->dispatch(nullos, 'G', 4, 'P', d, 0);
        }
    }

    // rapids retract at R-Plane (Initial-Z or R)
    THEDISPATCHER->dispatch(nullos, 'G', 0, 'Z', this->r_plane, 0);
}

bool Drillingcycles::handle_gcode(GCode& gcode, OutputStream& os)
{
    // "G" value
    int code = gcode.get_code();

    // cycle start
    if (code == 98 || code == 99) {
        // wait for any moves left and current position is update
        Conveyor::getInstance()->wait_for_idle();

        // get actual position from robot
        float pos[3];
        Robot::getInstance()->get_axis_position(pos);
        // convert to WCS
        Robot::wcs_t wpos = Robot::getInstance()->mcs2wcs(pos);
        // backup Z position as Initial-Z value
        this->initial_z = std::get<Z_AXIS>(wpos); // must use the work coordinate position
        // set retract type
        this->retract_type = (code == 98) ? RETRACT_TO_Z : RETRACT_TO_R;
        // reset sticky values
        this->reset_sticky();
        // mark cycle started and gcode taken
        this->cycle_started = true;

    } else if (code == 80) { // cycle end
        // mark cycle ended
        this->cycle_started = false;

        // if retract position is R-Plane
        if (this->retract_type == RETRACT_TO_R) {
            OutputStream nullos;
            // rapids retract at Initial-Z to avoid futur collisions
            THEDISPATCHER->dispatch(nullos, 'G', 0, 'Z', this->initial_z, 0);
        }

    } else if (this->cycle_started && (code == 81 || code == 82 || code == 83) ) { // in cycle
        // relative mode not supported for now...
        if (Robot::getInstance()->absolute_mode == false) {
            os.printf("Drillingcycles: relative mode not supported.\r\n");
            os.printf("Drillingcycles: skip hole...\r\n");
            // exit
            return false;
        }

        this->update_sticky(gcode);
        this->make_hole(gcode);

    } else {
        return false;
    }

    return true;
}
