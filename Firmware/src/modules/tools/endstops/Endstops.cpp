#include "Endstops.h"
#include "GCode.h"
#include "Conveyor.h"
#include "ActuatorCoordinates.h"
#include "Pin.h"
#include "StepperMotor.h"
#include "Robot.h"
#include "SlowTicker.h"
#include "Planner.h"
#include "OutputStream.h"
#include "StepTicker.h"
#include "ConfigReader.h"
#include "Dispatcher.h"
#include "main.h"
#include "BaseSolution.h"
#include "Consoles.h"
#include "benchmark_timer.h"

#include <ctype.h>
#include <algorithm>
#include <math.h>
#include <bitset>

// global config settings
#define corexy_homing_key "corexy_homing"
#define delta_homing_key "delta_homing"
#define rdelta_homing_key "rdelta_homing"
#define scara_homing_key "scara_homing"

#define debounce_ms_key "debounce_ms"

#define home_z_first_key "home_z_first"
#define homing_order_key "homing_order"
#define move_to_origin_key "move_to_origin_after_home"

#define alpha_trim_key "alpha_trim_mm"
#define beta_trim_key "beta_trim_mm"
#define gamma_trim_key "gamma_trim_mm"

#define pin_key "pin"
#define axis_key "axis"
#define direction_key "homing_direction"
#define position_key "homing_position"
#define fast_rate_key "fast_rate"
#define slow_rate_key "slow_rate"
#define max_travel_key "max_travel"
#define retract_key "retract"
#define limit_key "limit_enable"

#define STEPPER Robot::getInstance()->actuators
#define STEPS_PER_MM(a) (STEPPER[a]->get_steps_per_mm())


// Homing States
enum STATES {
    MOVING_TO_ENDSTOP_FAST, // homing move
    MOVING_TO_ENDSTOP_SLOW, // homing move
    MOVING_BACK,            // homing move
    NOT_HOMING,
    BACK_OFF_HOME,
    MOVE_TO_ORIGIN,
    LIMIT_TRIGGERED
};

REGISTER_MODULE(Endstops, Endstops::create)

bool Endstops::create(ConfigReader& cr)
{
    printf("DEBUG: configure endstops\n");
    Endstops *endstops = new Endstops();
    if(!endstops->configure(cr)) {
        printf("INFO: No endstops enabled\n");
        delete endstops;
        endstops = nullptr;
    }
    return true;
}

Endstops::Endstops() : Module("endstops")
{
    this->status = NOT_HOMING;
}

bool Endstops::configure(ConfigReader& cr)
{
    // load endstops
    if(!load_endstops(cr)) {
        return false;
    }

    SlowTicker::getInstance()->attach(100, std::bind(&Endstops::read_endstops, this));

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER, 28, std::bind(&Endstops::handle_G28, this, _1, _2));

    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 119, std::bind(&Endstops::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 206, std::bind(&Endstops::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 306, std::bind(&Endstops::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 500, std::bind(&Endstops::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 665, std::bind(&Endstops::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 666, std::bind(&Endstops::handle_mcode, this, _1, _2));

    return true;
}

// Get config using new syntax supports ABC
bool Endstops::load_endstops(ConfigReader& cr)
{
    limit_enabled = false;
    uint8_t max_index = 0;

    std::array<homing_info_t, k_max_actuators> temp_axis_array; // needs to be at least XYZ, but allow for ABC
    {
        homing_info_t t;
        t.axis = 0;
        t.axis_index = 0;
        t.pin_info = nullptr;
        temp_axis_array.fill(t);
    }

    // iterate over all in endstops section
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("endstops", ssmap)) {
        printf("INFO: configure-endstop: no section found\n");
        return false;
    }

    // keep track of axis that have been defined for homing
    std::bitset<6> home_defined;
    home_defined.reset();

    for(auto& s : ssmap) {

        // foreach endstop
        std::string name = s.first;
        if(name == "common") continue; // skip common settings here
        auto& mm = s.second;
        if(!cr.get_bool(mm, "enable", false)) continue;

        endstop_info_t *pin_info = new endstop_info_t;
        if(!pin_info->pin.from_string(cr.get_string(mm, pin_key, "nc")) || !pin_info->pin.as_input() || !pin_info->pin.connected()) {
            // no pin defined try next
            printf("ERROR: configure-endstop: no pin defined or illegal pin for %s\n", name.c_str());
            delete pin_info;
            continue;
        }

        std::string axis = cr.get_string(mm, axis_key, "");
        if(axis.empty()) {
            // axis is required
            printf("ERROR: configure-endstop: no axis defined for %s\n", name.c_str());
            delete pin_info;
            continue;
        }

        uint8_t a;
        switch(toupper(axis[0])) {
            case 'X': a = X_AXIS; break;
            case 'Y': a = Y_AXIS; break;
            case 'Z': a = Z_AXIS; break;
            case 'A': a = A_AXIS; break;
            case 'B': a = B_AXIS; break;
            case 'C': a = C_AXIS; break;
            default: // not a recognized axis
                printf("ERROR: configure-endstop: not a known axis %c, defined for %s\n", axis[0], name.c_str());
                delete pin_info;
                continue;
        }

        // init pin struct
        pin_info->debounce = 0;
        pin_info->axis = toupper(axis[0]);
        pin_info->axis_index = a;
        pin_info->home = false;

        // are limits enabled
        pin_info->limit_enable = cr.get_bool(mm, limit_key, false);
        limit_enabled |= pin_info->limit_enable;

        // enter into endstop array
        endstops.push_back(pin_info);

        // check we are not going above the number of configured actuators/axis
        if(a >= Robot::getInstance()->get_number_registered_motors()) {
            // too many axis we only have configured n_motors
            printf("WARNING: configure-endstop: Too many endstops defined for the number of axis\n");
            continue;
        }

        // keep track of the maximum index that has been defined
        if(a > max_index) max_index = a;

        // if set to none it means not used for homing (maybe limit only) so do not add to the homing array
        std::string direction = cr.get_string(mm, direction_key, "none");
        if(direction == "none") {
            continue;
        }

        // check this axis does not already have homing set (we can only have one homing direction set)
        if(home_defined[a]) {
            printf("ERROR: configure-endstop: axis %d already has a homing direction set\n", a);
            continue;
        }

        home_defined.set(a);

        // setup the homing array
        homing_info_t hinfo;

        // init homing struct
        hinfo.home_offset = 0;
        hinfo.homed = false;
        hinfo.axis = toupper(axis[0]);
        hinfo.axis_index = a;
        hinfo.pin_info = pin_info;
        pin_info->home = true;

        // rates in mm/sec
        hinfo.fast_rate = cr.get_float(mm, fast_rate_key, 100);
        hinfo.slow_rate = cr.get_float(mm, slow_rate_key, 10);

        // retract in mm
        hinfo.retract = cr.get_float(mm, retract_key, 5);

        // homing direction and convert to boolean where true is home to min, and false is home to max
        hinfo.home_direction =  direction == "home_to_min";

        // homing cartesian position
        hinfo.homing_position = cr.get_float(mm, position_key, hinfo.home_direction ? 0 : 200);

        // used to set maximum movement on homing, set by max_travel if defined
        hinfo.max_travel = cr.get_float(mm, max_travel_key, 500);

        // See if this axis has a slave and set it
        hinfo.slaved_axis = nullptr; // no slave is the default
#if MAX_ROBOT_ACTUATORS > 3
        if(a >= X_AXIS && a <= Z_AXIS) {
            if(STEPPER[a]->has_slave()) {
                hinfo.slaved_axis = STEPPER[a]->get_slave(); // set the slaved axis, needed to stop it when homed
            }
        }
#endif

        // stick into array in correct place
        temp_axis_array[hinfo.axis_index] = hinfo;
    }

    // if no pins defined then disable the module
    if(endstops.empty()) return false;

    homing_axis.shrink_to_fit();
    endstops.shrink_to_fit();

    // copy to the homing_axis array, make sure that undefined entries are filled in as well
    // as the order is important and all slots must be filled upto the max_index
    for (size_t i = 0; i < temp_axis_array.size(); ++i) {
        if(temp_axis_array[i].axis == 0) {
            // was not configured above, if it is XYZ then we need to force a dummy entry
            if(i <= Z_AXIS) {
                homing_info_t t;
                t.homed = false;
                t.axis = 'X' + i;
                t.axis_index = i;
                t.pin_info = nullptr; // this tells it that it cannot be used for homing
                homing_axis.push_back(t);

            } else if(i <= max_index) {
                // for instance case where we defined C without A or B
                homing_info_t t;
                t.axis = 'A' + i - 3;
                t.axis_index = i;
                t.pin_info = nullptr; // this tells it that it cannot be used for homing
                homing_axis.push_back(t);
            }


        } else {
            homing_axis.push_back(temp_axis_array[i]);
        }
    }

    // sets some endstop global configs applicable to all endstops
    auto s = ssmap.find("common");
    if(s != ssmap.end()) {
        auto& mm = s->second; // map of common endstop config settings

        this->debounce_ms = cr.get_float(mm, debounce_ms_key, 0); // 0 means no debounce

        this->is_corexy = cr.get_bool(mm, corexy_homing_key, false);
        this->is_delta =  cr.get_bool(mm, delta_homing_key, false);
        this->is_rdelta = cr.get_bool(mm, rdelta_homing_key, false);
        this->is_scara =  cr.get_bool(mm, scara_homing_key, false);

        this->home_z_first = cr.get_bool(mm, home_z_first_key, false);

        this->trim_mm[0] = cr.get_float(mm, alpha_trim_key, 0);
        this->trim_mm[1] = cr.get_float(mm, beta_trim_key, 0);
        this->trim_mm[2] = cr.get_float(mm, gamma_trim_key, 0);

        // see if an order has been specified, must be three or more characters, XYZABC or ABYXZ etc
        std::string order = cr.get_string(mm, homing_order_key, "");
        this->homing_order = 0;
        if(order.size() >= 3 && order.size() <= homing_axis.size() && !(this->is_delta || this->is_rdelta)) {
            int shift = 0;
            for(auto c : order) {
                char n = toupper(c);
                uint32_t i = n >= 'X' ? n - 'X' : n - 'A' + 3;
                i += 1; // So X is 1
                if(i > 6) { // bad value
                    this->homing_order = 0;
                    break;
                }
                homing_order |= (i << shift);
                shift += 3;
            }
        }

        // set to true by default for deltas due to trim, false on cartesians
        this->move_to_origin_after_home = cr.get_bool(mm, move_to_origin_key, is_delta);

    } else {
        printf("WARNING: configure-endstop: no common settings found. Using defaults\n");
        // set defaults
        this->debounce_ms = 0;
        this->is_corexy = false;
        this->is_delta =  false;
        this->is_rdelta = false;
        this->is_scara =  false;
        this->home_z_first = false;
        this->trim_mm[0] = 0;
        this->trim_mm[1] = 0;
        this->trim_mm[2] = 0;
        this->homing_order = 0;
        this->move_to_origin_after_home = is_delta;
    }

    return true;
}

// Called every 10 milliseconds from the timer thread
void Endstops::read_endstops()
{
    if(limit_enabled) check_limits();

    if(this->status != MOVING_TO_ENDSTOP_SLOW && this->status != MOVING_TO_ENDSTOP_FAST) return; // not doing anything we need to monitor for

    // check each homing endstop
    for(auto& e : homing_axis) { // check all axis homing endstops
        if(e.pin_info == nullptr) continue; // ignore if not a homing endstop
        int m = e.axis_index;

        // for corexy homing in X or Y we must only check the associated endstop, works as we only home one axis at a time for corexy
        if(is_corexy && (m == X_AXIS || m == Y_AXIS) && !axis_to_home[m]) continue;

        if(STEPPER[m]->is_moving()) {
            // if it is moving then we check the associated endstop, and debounce it
            if(e.pin_info->pin.get()) {
                if(e.pin_info->debounce < debounce_ms) {
                    e.pin_info->debounce += 10; // as each iteration is 10ms

                } else {
                    if(is_corexy && (m == X_AXIS || m == Y_AXIS)) {
                        // corexy when moving in X or Y we need to stop both the X and Y motors
                        STEPPER[X_AXIS]->stop_moving();
                        STEPPER[Y_AXIS]->stop_moving();

                    } else {
                        // we signal the motor to stop, which will preempt any moves on that axis
                        STEPPER[m]->stop_moving();
                    }
                    e.pin_info->triggered = true;
                }

            } else {
                // The endstop was not hit yet
                e.pin_info->debounce = 0;
            }
        }
    }

    return;
}

// this is called from read endstops every 10ms if limits are enabled
void Endstops::check_limits()
{
    if(this->status == LIMIT_TRIGGERED) {
        // if we were in limit triggered see if all have been cleared
        bool all_clear = true;
        for(auto& i : endstops) {
            if(i->limit_enable) {
                if(i->pin.get()) {
                    // still triggered, so exit
                    i->debounce = 0;
                    return;
                }

                if(i->debounce < debounce_ms) {
                    i->debounce += 10;
                    all_clear = false;
                }
            }
        }

        if(all_clear) {
            // clear the state
            this->status = NOT_HOMING;
            print_to_all_consoles("// NOTICE hard limits are now enabled\n");
        }

        return;

    } else if(this->status != NOT_HOMING) {
        // don't check while homing
        return;
    }

    for(auto& i : endstops) {
        if(i->limit_enable && STEPPER[i->axis_index]->is_moving()) {
            // check min and max endstops
            if(i->pin.get()) {
                if(i->debounce < debounce_ms) {
                    i->debounce += 10;

                } else {
                    // endstop triggered
                    // disables heaters and motors, ignores incoming Gcode and flushes block queue
                    broadcast_halt(true);
                    char report_string[132];
                    // this needs to go to all connected consoles
                    snprintf(report_string, sizeof(report_string), "ALARM: Hard limit %c%c was hit\n", STEPPER[i->axis_index]->which_direction() ? '+' : '-', i->axis);
                    print_to_all_consoles(report_string);
                    print_to_all_consoles("// WARNING hard limits are disabled until all have been cleared\n");

                    this->status = LIMIT_TRIGGERED;
                    i->debounce = 0;
                    return;
                }
            }
        }
    }
}

// if limit switches are enabled, then we must move off of the endstop otherwise we won't be able to move
// checks if triggered and only backs off if triggered
void Endstops::back_off_home(axis_bitmap_t axis)
{
    this->status = BACK_OFF_HOME;
    float deltas[3] {0, 0, 0};
    float slow_rate = 0; // default mm/sec
    bool delta_set= false;

    // these are handled differently
    if(is_delta) {
        // Move off of the endstop using a regular relative move in Z only
        deltas[Z_AXIS] = homing_axis[Z_AXIS].retract * (homing_axis[Z_AXIS].home_direction ? 1 : -1);
        slow_rate = homing_axis[Z_AXIS].slow_rate;
        delta_set= true;

    } else {
        // cartesians concatenate all the moves we need to do into one move
        for( auto& e : homing_axis) {
            if(!axis[e.axis_index] || e.axis_index > Z_AXIS) continue; // only for axes we asked to move and X,Y,Z

            if(e.pin_info != nullptr && e.pin_info->limit_enable) {
                // debounce if needed
                safe_sleep(debounce_ms);
                // if not triggered no need to move off
                if(e.pin_info->pin.get()) {
                    deltas[e.axis_index] = e.retract * (e.home_direction ? 1 : -1);
                    // select slowest of them all
                    slow_rate = (slow_rate == 0) ? e.slow_rate : std::min(slow_rate, e.slow_rate);
                    delta_set= true;
                }
            }
        }
    }

    if(delta_set) {
        // Move off of the endstop using a delta relative move
        Robot::getInstance()->delta_move(deltas, slow_rate, 3);
        // Wait for above to finish
        Conveyor::getInstance()->wait_for_idle();
    }

    this->status = NOT_HOMING;
}

// If enabled will move the head to MCS 0,0 after homing, but only if X and Y were set to home
void Endstops::move_to_origin(axis_bitmap_t axis)
{
    if(!is_delta && (!axis[X_AXIS] || !axis[Y_AXIS])) return; // ignore if X and Y not homing, unless delta

    // Do we need to check if we are already at 0,0? probably not as the G0 will not do anything if we are
    // float pos[3]; Robot::getInstance()->get_axis_position(pos); if(pos[0] == 0 && pos[1] == 0) return;

    this->status = MOVE_TO_ORIGIN;

    // Move to center using a regular move, use slower of X and Y fast rate in mm/sec
    float rate = std::min(homing_axis[X_AXIS].fast_rate, homing_axis[Y_AXIS].fast_rate) * 60.0F;

    Robot::getInstance()->push_state();

    Robot::getInstance()->absolute_mode = true;
    Robot::getInstance()->next_command_is_MCS = true; // must use machine coordinates in case G92 or WCS is in effect
    OutputStream nullos;
    THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', 0.0F, 'Y', 0.0F, 'F', Robot::getInstance()->from_millimeters(rate), 0);

    Robot::getInstance()->pop_state();

    // Wait for above to finish
    Conveyor::getInstance()->wait_for_idle();
    this->status = NOT_HOMING;
}

void Endstops::home_xy()
{
    if(axis_to_home[X_AXIS] && axis_to_home[Y_AXIS]) {
        // Home XY first so as not to slow them down by homing Z at the same time
        float delta[3] {homing_axis[X_AXIS].max_travel, homing_axis[Y_AXIS].max_travel, 0};
        if(homing_axis[X_AXIS].home_direction) delta[X_AXIS] = -delta[X_AXIS];
        if(homing_axis[Y_AXIS].home_direction) delta[Y_AXIS] = -delta[Y_AXIS];
        float feed_rate = std::min(homing_axis[X_AXIS].fast_rate, homing_axis[Y_AXIS].fast_rate);
        Robot::getInstance()->delta_move(delta, feed_rate, 3);

    } else if(axis_to_home[X_AXIS]) {
        // now home X only
        float delta[3] {homing_axis[X_AXIS].max_travel, 0, 0};
        if(homing_axis[X_AXIS].home_direction) delta[X_AXIS] = -delta[X_AXIS];
        Robot::getInstance()->delta_move(delta, homing_axis[X_AXIS].fast_rate, 3);

    } else if(axis_to_home[Y_AXIS]) {
        // now home Y only
        float delta[3] {0,  homing_axis[Y_AXIS].max_travel, 0};
        if(homing_axis[Y_AXIS].home_direction) delta[Y_AXIS] = -delta[Y_AXIS];
        Robot::getInstance()->delta_move(delta, homing_axis[Y_AXIS].fast_rate, 3);
    }

    // Wait for axis to have homed
    Conveyor::getInstance()->wait_for_idle();
}

void Endstops::home(axis_bitmap_t a)
{
    // reset debounce counts for all endstops
    for(auto& e : endstops) {
        e->debounce = 0;
        e->triggered = false;
    }

    if (is_scara) {
        Robot::getInstance()->disable_arm_solution = true;  // Polar bots has to home in the actuator space.  Arm solution disabled.
    }

    this->axis_to_home = a;

    // Start moving the axes to the origin
    this->status = MOVING_TO_ENDSTOP_FAST;

    // delta_move does not do segmentation so this is not needed
    //Robot::getInstance()->disable_segmentation = true; // we must disable segmentation as this won't work with it enabled

    if(!home_z_first) home_xy();

    if(axis_to_home[Z_AXIS]) {
        // now home z
        float delta[3] {0, 0, homing_axis[Z_AXIS].max_travel}; // we go the max z
        if(homing_axis[Z_AXIS].home_direction) delta[Z_AXIS] = -delta[Z_AXIS];
        Robot::getInstance()->delta_move(delta, homing_axis[Z_AXIS].fast_rate, 3);
        // wait for Z
        Conveyor::getInstance()->wait_for_idle();
    }

    if(home_z_first) home_xy();

    // potentially home A B and C individually
    if(homing_axis.size() > 3) {
        for (size_t i = A_AXIS; i < homing_axis.size(); ++i) {
            if(axis_to_home[i]) {
                // now home A B or C
                float delta[i + 1];
                for (size_t j = 0; j <= i; ++j) delta[j] = 0;
                delta[i] = homing_axis[i].max_travel; // we go the max
                if(homing_axis[i].home_direction) delta[i] = -delta[i];
                Robot::getInstance()->delta_move(delta, homing_axis[i].fast_rate, i + 1);
                // wait for it
                Conveyor::getInstance()->wait_for_idle();
            }
        }
    }

    if(Module::is_halted()) {
        //Robot::getInstance()->disable_segmentation = false;
        Robot::getInstance()->disable_arm_solution = false;
        this->status = NOT_HOMING;
        return;
    }

    // check that the endstops were hit and it did not stop short for some reason
    // if the endstop is not triggered then enter ALARM state
    // with deltas we check all three axis were triggered, but at least one of XYZ must be set to home
    if(axis_to_home[X_AXIS] || axis_to_home[Y_AXIS] || axis_to_home[Z_AXIS]) {
        for (size_t i = X_AXIS; i <= Z_AXIS; ++i) {
            if((axis_to_home[i] || this->is_delta || this->is_rdelta) && !homing_axis[i].pin_info->triggered) {
                this->status = NOT_HOMING;
                broadcast_halt(true);
                //Robot::getInstance()->disable_segmentation = false;
                Robot::getInstance()->disable_arm_solution = false;
                return;
            }
        }
    }

    // also check ABC
    if(homing_axis.size() > 3) {
        for (size_t i = A_AXIS; i < homing_axis.size(); ++i) {
            if(axis_to_home[i] && !homing_axis[i].pin_info->triggered) {
                this->status = NOT_HOMING;
                broadcast_halt(true);
                //Robot::getInstance()->disable_segmentation = false;
                Robot::getInstance()->disable_arm_solution = false;
                return;
            }
        }
    }

    if (!is_scara) {
        // Only for non polar bots
        // we did not complete movement the full distance if we hit the endstops
        // TODO Maybe only reset axis involved in the homing cycle
        Robot::getInstance()->reset_position_from_current_actuator_position();
    }

    // Move back a small distance for all homing axis
    this->status = MOVING_BACK;
    float delta[homing_axis.size()];
    for (size_t i = 0; i < homing_axis.size(); ++i) delta[i] = 0;

    // use minimum feed rate of all axes that are being homed (sub optimal, but necessary)
    float feed_rate = 0;
    for (auto& i : homing_axis) {
        int c = i.axis_index;
        if(axis_to_home[c]) {
            delta[c] = i.retract;
            if(!i.home_direction) delta[c] = -delta[c];
            feed_rate = (feed_rate == 0) ? i.slow_rate : std::min(i.slow_rate, feed_rate);
        }
    }

    Robot::getInstance()->delta_move(delta, feed_rate, homing_axis.size());
    // wait until finished
    Conveyor::getInstance()->wait_for_idle();

    // Start moving the axes towards the endstops slowly
    this->status = MOVING_TO_ENDSTOP_SLOW;
    for (auto& i : homing_axis) {
        int c = i.axis_index;
        if(axis_to_home[c]) {
            delta[c] = i.retract * 2; // move further than we moved off to make sure we hit it cleanly
            if(i.home_direction) delta[c] = -delta[c];
        } else {
            delta[c] = 0;
        }
    }
    Robot::getInstance()->delta_move(delta, feed_rate, homing_axis.size());
    // wait until finished
    Conveyor::getInstance()->wait_for_idle();

    // we did not complete movement the full distance if we hit the endstops
    // TODO Maybe only reset axis involved in the homing cycle
    Robot::getInstance()->reset_position_from_current_actuator_position();

    //Robot::getInstance()->disable_segmentation = false;
    Robot::getInstance()->disable_arm_solution = false;  // Arm solution enabled again if it was disabled

    this->status = NOT_HOMING;
}

void Endstops::process_home_command(GCode& gcode, OutputStream& os)
{
    // First wait for the queue to be empty
    Conveyor::getInstance()->wait_for_idle();

    // turn off any compensation transform so Z does not move as XY home
    auto savect = Robot::getInstance()->compensationTransform;
    Robot::getInstance()->reset_compensated_machine_position();

    // deltas always home Z axis only, which moves all three actuators
    bool home_in_z_only = this->is_delta || this->is_rdelta;

    // figure out which axis to home
    axis_bitmap_t haxis;
    haxis.reset();

    bool axis_speced = (gcode.has_arg('X') || gcode.has_arg('Y') || gcode.has_arg('Z') ||
                        gcode.has_arg('A') || gcode.has_arg('B') || gcode.has_arg('C'));

    if(!home_in_z_only) { // ie not a delta
        for (auto &p : homing_axis) {
            // only enable homing if the endstop is defined,
            if(p.pin_info == nullptr) continue;
            if(!axis_speced || gcode.has_arg(p.axis)) {
                haxis.set(p.axis_index);
                p.homed = false;
                // now reset axis to 0 as we do not know what state we are in
                if (!is_scara) {
                    Robot::getInstance()->reset_axis_position(0, p.axis_index);
                } else {
                    // SCARA resets arms to plausable minimum angles
                    Robot::getInstance()->reset_axis_position(-30, 30, 0); // angles set into axis space for homing.
                }
            }
        }

    } else {
        bool home_z = !axis_speced || gcode.has_arg('X') || gcode.has_arg('Y') || gcode.has_arg('Z');

        // if we specified an axis we check ABC
        for (size_t i = A_AXIS; i < homing_axis.size(); ++i) {
            auto &p = homing_axis[i];
            if(p.pin_info == nullptr) continue;
            if(!axis_speced || gcode.has_arg(p.axis)) {
                haxis.set(p.axis_index);
                p.homed = false;
            }
        }

        if(home_z) {
            // Only Z axis homes (even though all actuators move this is handled by arm solution)
            haxis.set(Z_AXIS);
            homing_axis[X_AXIS].homed = false;
            homing_axis[Y_AXIS].homed = false;
            homing_axis[Z_AXIS].homed = false;

            // we also set the kinematics to a known good position, this is necessary for a rotary delta, but doesn't hurt for linear delta
            Robot::getInstance()->reset_axis_position(0, 0, 0);
        }
    }

    if(haxis.none()) {
        printf("WARNING: Nothing to home\n");
        // restore compensationTransform
        Robot::getInstance()->compensationTransform = savect;
        return;
    }

    // do the actual homing
    if(homing_order != 0 && !is_scara) {
        // if an order has been specified do it in the specified order
        // homing order is 0bfffeeedddcccbbbaaa where aaa is 1,2,3,4,5,6 to specify the first axis (XYZABC), bbb is the second and ccc is the third etc
        // eg 0b0101011001010 would be Y X Z A, 011 010 001 100 101 would be  B A X Y Z
        for (uint32_t m = homing_order; m != 0; m >>= 3) {
            uint32_t a = (m & 0x07) - 1; // axis to home
            if(a < homing_axis.size() && haxis[a]) { // if axis is selected to home
                axis_bitmap_t bs;
                bs.set(a);
                home(bs);
            }
            // check if on_halt (eg kill)
            if(Module::is_halted()) break;
        }

    } else if(is_corexy) {
        // corexy must home each axis individually
        for (auto &p : homing_axis) {
            if(haxis[p.axis_index]) {
                axis_bitmap_t bs;
                bs.set(p.axis_index);
                home(bs);
            }
            // check if on_halt (eg kill)
            if(Module::is_halted()) break;
        }

    } else {
        // they could all home at the same time
        home(haxis);
    }

    // restore compensationTransform
    Robot::getInstance()->compensationTransform = savect;

    // check if on_halt (eg kill or fail)
    if(Module::is_halted()) {
        if(!THEDISPATCHER->is_grbl_mode()) {
            os.printf("ERROR: Homing cycle failed\n");
        } else {
            os.printf("ALARM: Homing fail\n");
        }
        // clear all the homed flags
        for (auto &p : homing_axis) p.homed = false;
        return;
    }

    if(home_in_z_only || is_scara) { // deltas and scaras only
        // Here's where we would have been if the endstops were perfectly trimmed
        // NOTE on a rotary delta home_offset is actuator position in degrees when homed and
        // home_offset is the theta offset for each actuator, so M206 is used to set theta offset for each actuator in degrees
        // FIXME not sure this will work with compensation transforms on.
        float ideal_position[3] = {
            homing_axis[X_AXIS].homing_position + homing_axis[X_AXIS].home_offset,
            homing_axis[Y_AXIS].homing_position + homing_axis[Y_AXIS].home_offset,
            homing_axis[Z_AXIS].homing_position + homing_axis[Z_AXIS].home_offset
        };

        bool has_endstop_trim = this->is_delta || is_scara;
        if (has_endstop_trim) {
            ActuatorCoordinates ideal_actuator_position;
            Robot::getInstance()->arm_solution->cartesian_to_actuator(ideal_position, ideal_actuator_position);

            // We are actually not at the ideal position, but a trim away
            ActuatorCoordinates real_actuator_position = {
                ideal_actuator_position[X_AXIS] - this->trim_mm[X_AXIS],
                ideal_actuator_position[Y_AXIS] - this->trim_mm[Y_AXIS],
                ideal_actuator_position[Z_AXIS] - this->trim_mm[Z_AXIS]
            };

            float real_position[3];
            Robot::getInstance()->arm_solution->actuator_to_cartesian(real_actuator_position, real_position);
            // Reset the actuator positions to correspond to our real position
            Robot::getInstance()->reset_axis_position(real_position[0], real_position[1], real_position[2]);

        } else {
            // without endstop trim, real_position == ideal_position
            if(is_rdelta) {
                // with a rotary delta we set the actuators angle then use the FK to calculate the resulting cartesian coordinates
                ActuatorCoordinates real_actuator_position = {ideal_position[0], ideal_position[1], ideal_position[2]};
                Robot::getInstance()->reset_actuator_position(real_actuator_position);

            } else {
                // Reset the actuator positions to correspond to our real position
                Robot::getInstance()->reset_axis_position(ideal_position[0], ideal_position[1], ideal_position[2]);
            }
        }

        // for deltas we say all 3 axis are homed even though it was only Z
        homing_axis[X_AXIS].homed = true;
        homing_axis[Y_AXIS].homed = true;
        homing_axis[Z_AXIS].homed = true;

        // if we also homed ABC then we need to reset them
        for (size_t i = A_AXIS; i < homing_axis.size(); ++i) {
            auto &p = homing_axis[i];
            if (haxis[p.axis_index]) { // if we requested this axis to home
                Robot::getInstance()->reset_axis_position(p.homing_position + p.home_offset, p.axis_index);
                // set flag indicating axis was homed, it stays set once set until H/W reset or unhomed
                p.homed = true;
            }
        }

    } else {
        // Zero the ax(i/e)s position, add in the home offset
        // NOTE that if compensation is active the Z will be set based on where XY are, so make sure XY are homed first then Z
        // so XY are at a known consistent position.  (especially true if using a proximity probe)
        for (auto &p : homing_axis) {
            if (haxis[p.axis_index]) { // if we requested this axis to home
                Robot::getInstance()->reset_axis_position(p.homing_position + p.home_offset, p.axis_index);
                // set flag indicating axis was homed, it stays set once set until H/W reset or unhomed
                p.homed = true;
            }
        }
    }

    // on some systems where 0,0 is bed center it is nice to have home goto 0,0 after homing
    // default is off for cartesian on for deltas
    if(!is_delta) {
        // NOTE a rotary delta usually has optical or hall-effect endstops so it is safe to go past them a little bit
        if(this->move_to_origin_after_home) move_to_origin(haxis);
        // if limit switches are enabled we must back off endstop after setting home
        back_off_home(haxis);

    } else if(haxis[Z_AXIS] && (this->move_to_origin_after_home || homing_axis[X_AXIS].pin_info->limit_enable)) {
        // deltas are not left at 0,0 because of the trim settings, so move to 0,0 if requested, but we need to back off endstops first
        // also need to back off endstops if limits are enabled
        back_off_home(haxis);
        if(this->move_to_origin_after_home) move_to_origin(haxis);
    }
}

void Endstops::set_homing_offset(GCode& gcode, OutputStream& os)
{
    // M306 Similar to M206 but sets Homing offsets based on current MCS position
    // Basically it finds the delta between the current MCS position and the requested position and adds it to the homing offset
    // then will not let it be set again until that axis is homed.
    float pos[3];
    Robot::getInstance()->get_axis_position(pos);

    if (gcode.has_arg('X')) {
        if(!homing_axis[X_AXIS].homed) {
            os.printf("error: Axis X must be homed before setting Homing offset\n");
            return;
        }
        homing_axis[X_AXIS].home_offset += (Robot::getInstance()->to_millimeters(gcode.get_arg('X')) - pos[X_AXIS]);
        homing_axis[X_AXIS].homed = false; // force it to be homed
    }
    if (gcode.has_arg('Y')) {
        if(!homing_axis[Y_AXIS].homed) {
            os.printf("error: Axis Y must be homed before setting Homing offset\n");
            return;
        }
        homing_axis[Y_AXIS].home_offset += (Robot::getInstance()->to_millimeters(gcode.get_arg('Y')) - pos[Y_AXIS]);
        homing_axis[Y_AXIS].homed = false; // force it to be homed
    }
    if (gcode.has_arg('Z')) {
        if(!homing_axis[Z_AXIS].homed) {
            os.printf("error: Axis Z must be homed before setting Homing offset\n");
            return;
        }
        homing_axis[Z_AXIS].home_offset += (Robot::getInstance()->to_millimeters(gcode.get_arg('Z')) - pos[Z_AXIS]);
        homing_axis[Z_AXIS].homed = false; // force it to be homed
    }

    os.printf("// Homing Offset: X %5.3f Y %5.3f Z %5.3f will take effect next home\n", homing_axis[X_AXIS].home_offset, homing_axis[Y_AXIS].home_offset, homing_axis[Z_AXIS].home_offset);
}

// handle G28
bool Endstops::handle_G28(GCode& gcode, OutputStream& os)
{
    switch(gcode.get_subcode()) {
        case 0: // G28 in grbl mode will do a rapid to the predefined position otherwise it is home command
            if(!THEDISPATCHER->is_grbl_mode()) {
                process_home_command(gcode, os);
            } else {
                // park handled in Robot
                return false;
            }
            break;

        case 2: // G28.2 in grbl mode does homing (triggered by $H), otherwise it moves to the park position
            if(THEDISPATCHER->is_grbl_mode()) {
                process_home_command(gcode, os);
            } else {
                // park handled in Robot
                return false;
            }
            break;

        case 3: // G28.3 is a smoothie special it sets manual homing
            if(gcode.get_num_args() == 0) {
                for (auto &p : homing_axis) {
                    p.homed = true;
                    Robot::getInstance()->reset_axis_position(0, p.axis_index);
                }
            } else {
                // do a manual homing based on given coordinates
                if(gcode.has_arg('X')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('X'), X_AXIS); homing_axis[X_AXIS].homed = true; }
                if(gcode.has_arg('Y')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('Y'), Y_AXIS); homing_axis[Y_AXIS].homed = true; }
                if(gcode.has_arg('Z')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('Z'), Z_AXIS); homing_axis[Z_AXIS].homed = true; }
                if(homing_axis.size() > A_AXIS && gcode.has_arg('A')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('A'), A_AXIS); homing_axis[A_AXIS].homed = true; }
                if(homing_axis.size() > B_AXIS && gcode.has_arg('B')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('B'), B_AXIS); homing_axis[B_AXIS].homed = true; }
                if(homing_axis.size() > C_AXIS && gcode.has_arg('C')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('C'), C_AXIS); homing_axis[C_AXIS].homed = true; }
            }
            break;

        case 4: { // G28.4 is a smoothie special it sets manual homing based on the actuator position (used for rotary delta)
            // do a manual homing based on given coordinates, no endstops required
            ActuatorCoordinates ac{NAN, NAN, NAN};
            if(gcode.has_arg('X')) { ac[0] =  gcode.get_arg('X'); homing_axis[X_AXIS].homed = true; }
            if(gcode.has_arg('Y')) { ac[1] =  gcode.get_arg('Y'); homing_axis[Y_AXIS].homed = true; }
            if(gcode.has_arg('Z')) { ac[2] =  gcode.get_arg('Z'); homing_axis[Z_AXIS].homed = true; }
            Robot::getInstance()->reset_actuator_position(ac);
        }
        break;

        case 5: // G28.5 is a smoothie special it clears the homed flag for the specified axis, or all if not specifed
            if(gcode.get_num_args() == 0) {
                for (auto &p : homing_axis) p.homed = false;
            } else {
                if(gcode.has_arg('X')) homing_axis[X_AXIS].homed = false;
                if(gcode.has_arg('Y')) homing_axis[Y_AXIS].homed = false;
                if(gcode.has_arg('Z')) homing_axis[Z_AXIS].homed = false;
                if(homing_axis.size() > A_AXIS && gcode.has_arg('A')) homing_axis[A_AXIS].homed = false;
                if(homing_axis.size() > B_AXIS && gcode.has_arg('B')) homing_axis[B_AXIS].homed = false;
                if(homing_axis.size() > C_AXIS && gcode.has_arg('C')) homing_axis[C_AXIS].homed = false;
            }
            break;

        case 6: // G28.6 is a smoothie special it shows the homing status of each axis
            for (auto &p : homing_axis) {
                os.printf("%c:%d ", p.axis, p.homed);
            }
            os.set_append_nl();
            break;

        case 7: // G28.7 is a smoothie special it homes a slaved axis and optionally adjusts it to the stored trim value
            if(gcode.get_num_args() != 1) {
                os.printf("G28.7 requires one axis argument (XYZ)\n");

            }else{
                int a= -1;
                bool adjust= false;
                if(gcode.has_arg('X')) { a= 0; adjust= gcode.get_arg('X') != 0; }
                else if(gcode.has_arg('Y')) { a= 1; adjust= gcode.get_arg('Y') != 0; }
                else if(gcode.has_arg('Z')) { a= 2; adjust= gcode.get_arg('Z') != 0; }
                else os.printf("axis argument must be one of XYZ\n");

                if(a >= 0) {
                    move_slaved_axis(a, adjust, os);
                }
            }
            break;

        default:
            return false;
    }

    return true;
}

bool Endstops::handle_mcode(GCode& gcode, OutputStream& os)
{
    switch (gcode.get_code()) {
        case 119: {
            os.set_append_nl();
            for(auto& h : homing_axis) {
                if(h.pin_info == nullptr) continue; // not a configured homing endstop
                std::string name;
                name.append(1, h.axis).append(h.home_direction ? "_min" : "_max");
                os.printf("%s:%d ", name.c_str(), h.pin_info->pin.get());
            }
            os.printf("pins- ");
            for(auto& p : endstops) {
                std::string str(1, p->axis);
                if(p->home) str.append(":H");
                if(p->limit_enable) str.append(":L");
                os.printf("(%s)%s ", str.c_str(), p->pin.to_string().c_str());
            }
        }
        break;

        case 206: // M206 - set homing offset
            for (auto &p : homing_axis) {
                if(p.pin_info == nullptr) continue; // not a configured homing endstop
                if (gcode.has_arg(p.axis)) p.home_offset = gcode.get_arg(p.axis);
            }

            for (auto &p : homing_axis) {
                if(p.pin_info == nullptr) continue; // not a configured homing endstop
                os.printf("%c: %5.3f ", p.axis, p.home_offset);
            }

            os.printf(" will take effect next home\n");
            break;

        case 306: // set homing offset based on current position
            if(is_rdelta) {
                rotary_delta_M306(gcode, os);
            } else {
                set_homing_offset(gcode, os);
            }
            break;

        case 500: // save settings
            if(!is_rdelta) {
                os.printf(";Home offset (mm):\nM206 ");
                for (auto &p : homing_axis) {
                    if(p.pin_info == nullptr) continue; // not a configured homing endstop
                    os.printf("%c%1.2f ", p.axis, p.home_offset);
                }
                os.printf("\n");

            } else {
                os.printf(";Theta offset (degrees):\nM206 X%1.5f Y%1.5f Z%1.5f\n",
                          homing_axis[X_AXIS].home_offset, homing_axis[Y_AXIS].home_offset, homing_axis[Z_AXIS].home_offset);
            }

            if(trim_mm[0] != 0 || trim_mm[1] != 0 || trim_mm[2] != 0) {
                os.printf(";Trim (mm):\nM666 X%1.4f Y%1.4f Z%1.4f\n", trim_mm[0], trim_mm[1], trim_mm[2]);
            }
            if (this->is_delta || this->is_scara) {
                os.printf(";Max Z\nM665 Z%1.3f\n", homing_axis[Z_AXIS].homing_position);
            }
            break;

        case 665:
            if (this->is_delta || this->is_scara) { // M665 - set max gamma/z height
                os.set_append_nl();
                float gamma_max = homing_axis[Z_AXIS].homing_position;
                if (gcode.has_arg('Z')) {
                    homing_axis[Z_AXIS].homing_position = gamma_max = gcode.get_arg('Z');
                }
                os.printf("Max Z: %8.3f ", gamma_max);
            }
            break;

        case 666:
            // M666 - set trim for each axis in mm, NB negative mm trim is down
            if (gcode.has_arg('X')) trim_mm[0] = gcode.get_arg('X');
            if (gcode.has_arg('Y')) trim_mm[1] = gcode.get_arg('Y');
            if (gcode.has_arg('Z')) trim_mm[2] = gcode.get_arg('Z');

            // print the current trim values in mm
            os.printf("X: %5.4f Y: %5.4f Z: %5.4f\n", trim_mm[0], trim_mm[1], trim_mm[2]);
            break;

        default:
            return false;
    }

    return true;
}

bool Endstops::request(const char *key, void *value)
{
    if(strcmp(key, "get_homing_status") == 0) {
        bool *homing = static_cast<bool *>(value);
        *homing = this->status != NOT_HOMING && this->status != LIMIT_TRIGGERED;
        return true;
    }

    if(strcmp(key, "is_homed") == 0) {
        bool *homed = static_cast<bool *>(value);
        *homed = true;
        for (auto &p : homing_axis) {
            if(p.pin_info != nullptr && !p.homed) {
                *homed = false;
                break;
            }
        }

        return true;
    }

    if(strcmp(key, "can_z_home") == 0) {
        bool *b = static_cast<bool *>(value);
        *b = homing_axis[Z_AXIS].pin_info != nullptr;
        return true;
    }

    if(strcmp(key, "clear_homed") == 0) {
        for (auto &p : homing_axis) {
            if(p.pin_info != nullptr && p.homed) {
                p.homed = false;
            }
        }

        return true;
    }

    if(strcmp(key, "get_trim") == 0) {
        float *trim = static_cast<float *>(value);
        for (int i = 0; i < 3; ++i) {
            trim[i] = this->trim_mm[i];
        }
        return true;
    }

    if(strcmp(key, "get_home_offset") == 0) {
        // provided by caller
        float *data = static_cast<float *>(value);
        for (int i = 0; i < 3; ++i) {
            data[i] = homing_axis[i].home_offset;
        }
        return true;
    }

    // sets
    if(strcmp(key, "set_trim") == 0) {
        float *t = static_cast<float*>(value);
        this->trim_mm[0] = t[0];
        this->trim_mm[1] = t[1];
        this->trim_mm[2] = t[2];
        return true;
    }

    if(strcmp(key, "set_home_offset") == 0) {
        float *t = static_cast<float*>(value);
        if(!isnan(t[0])) homing_axis[0].home_offset = t[0];
        if(!isnan(t[1])) homing_axis[1].home_offset = t[1];
        if(!isnan(t[2])) homing_axis[2].home_offset = t[2];
        return true;
    }


    return false;
}

void Endstops::rotary_delta_M306(GCode& gcode, OutputStream& os)
{
    // for a rotary delta M306 calibrates the homing angle
    // by doing M306 X-56.17 it will calculate the M206 X value (the theta offset for actuator X) based on the difference
    // between what it thinks is the current angle and what the current angle actually is specified by X (ditto for Y and Z)

    // get the current angle for each actuator, relies on being left where probe triggered (G30.1)
    // NOTE we only deal with XYZ so if there are more than 3 actuators this will probably go wonky
    ActuatorCoordinates current_angle = {
        Robot::getInstance()->actuators[0]->get_current_position(),
        Robot::getInstance()->actuators[1]->get_current_position(),
        Robot::getInstance()->actuators[2]->get_current_position()
    };

#if 0
    // FIXME the last probe is not in degrees for a rotary, so this is no longer valid
    if (gcode.has_arg('L') && gcode.get_arg('L') != 0) {
        // specifying L1 it will take the last probe position (set by G30) which is degrees moved
        // we convert that to actual postion of the actuator when it triggered.
        // this allows the use of G30 to find the angle tool and M306 L1 X-37.1234 to set X
        uint8_t ok;
        float d;
        // only Z is set by G30, but all will be the same amount moved
        std::tie(std::ignore, std::ignore, d, ok) = Robot::getInstance()->get_last_probe_position();
        if(ok == 0) {
            os.printf("error:Nothing set as probe failed or not run\n");
            return;
        }
        // presuming we returned to original position we need to change current angle by the amount moved
        current_angle[0] += d;
        current_angle[1] += d;
        current_angle[2] += d;
    }
#endif

    float theta_offset[3];
    for (int i = 0; i < 3; ++i) {
        theta_offset[i] = homing_axis[i].home_offset;
    }

    int cnt = 0;

    // figure out what home_offset needs to be to correct the homing_position
    if (gcode.has_arg('X')) {
        float a = gcode.get_arg('X'); // what actual angle is
        theta_offset[0] += (a - current_angle[0]);
        current_angle[0] = a;
        cnt++;
    }
    if (gcode.has_arg('Y')) {
        float b = gcode.get_arg('Y');
        theta_offset[1] += (b - current_angle[1]);
        current_angle[1] = b;
        cnt++;
    }
    if (gcode.has_arg('Z')) {
        float c = gcode.get_arg('Z');
        theta_offset[2] += (c - current_angle[2]);
        current_angle[2] = c;
        cnt++;
    }

    for (int i = 0; i < 3; ++i) {
        homing_axis[i].home_offset = theta_offset[i];
    }

    // reset the actuator positions (and machine position accordingly)
    // But only if all three actuators have been specified at the same time
    if(cnt == 3 || (gcode.has_arg('R') && gcode.get_arg('R') != 0)) {
        Robot::getInstance()->reset_actuator_position(current_angle);
        os.printf("NOTE: actuator position reset\n");
    } else {
        os.printf("NOTE: actuator position NOT reset\n");
    }

    os.printf("Theta Offset: X %8.5f Y %8.5f Z %8.5f\n", theta_offset[0], theta_offset[1], theta_offset[2]);
}

// For the given primary axis find the slave actuator and move it until the associated endstop is hit
// special function for moving the slaved actuator to align an axis with two actuators
// presumes the primary axis has already been homed.
bool Endstops::move_slaved_axis(uint8_t paxis, bool adjust, OutputStream& os)
{
    // check it is a slaved axis and get the associated primary axis
    StepperMotor *sa= homing_axis[paxis].slaved_axis;
    if(sa != nullptr) {
        if(!homing_axis[paxis].homed) {
            // the primary axis has not been homed so bail
            os.printf("error: The primary axis %c (%d) has not been homed\n", homing_axis[paxis].axis,  paxis);
            return false;
        }

    } else {
        os.printf("error: axis %c (%d) does not have a slaved axis\n", homing_axis[paxis].axis, paxis);
        return false;
    }

    // use slow feedrate in mm/sec of the primary axis
    float fr = homing_axis[paxis].slow_rate;
    // The maximum travel for the slaved actuator needs to be quite small to avoid causing damage
    float max_travel= 20; // maximum travel in mm (TODO may need to be configurable)
    uint32_t steps = lroundf(Robot::getInstance()->actuators[paxis]->get_steps_per_mm() * max_travel);
    // convert fr to steps/sec
    uint32_t sps = lroundf(Robot::getInstance()->actuators[paxis]->get_steps_per_mm() * fr);
    sps = std::max(sps, (uint32_t)1); // make sure it is at least 1 step/sec

    // find the endstop pin for the slaved axis, it will be the endstop for the A (or B/C) axis
    endstop_info_t *es= nullptr;
    for(endstop_info_t *e : endstops) {
        if(e->axis_index != sa->get_motor_id()) continue;

        if(e->limit_enable) {
            os.printf("error: limit must not be enabled for the endstop %d\n", sa->get_motor_id());
            return false;
        }

        es= e;
    }

    if(es == nullptr) {
        os.printf("error: The slaved axis %d has no assigned endstop\n", sa->get_motor_id());
        return false;

    }

    // if already triggered explain that it needs to be moved away slightly
    // TODO this may be fixed in a later version and allow for it to be triggered
    if(es->pin.get()) {
        os.printf("error: endstop %d is already triggered, please adjust so it is not triggered at the same time as the primary endstop\n", sa->get_motor_id());
        return false;
    }

    // move in the same direction as the homing cycle would move
    uint32_t nsteps;
    bool dir= homing_axis[paxis].home_direction;

    os.printf("issuing %d steps at a rate of %d steps/sec on the %c axis, direction %d\n", steps, sps, sa->get_motor_id(), dir);
    bool hit= manual_move(steps, sps, sa, dir, nsteps, &es->pin);

    if(!hit) {
        os.printf("error: endstop %d was not triggered, please make sure it is within %f mm\n", sa->get_motor_id(), max_travel);

    }else{

        // find the offset moved from aligned
        float offset= nsteps / Robot::getInstance()->actuators[paxis]->get_steps_per_mm();
        os.printf("Actuator %d, moved %lu steps, %0.4f mm to endstop\n", sa->get_motor_id(), nsteps, offset);

        if(adjust && trim_mm[paxis] != 0) {
            // if trim is set then move back that amount to realign the axis
            nsteps = lroundf(Robot::getInstance()->actuators[paxis]->get_steps_per_mm() * trim_mm[paxis]);
            os.printf("Actuator %d, adjusted %lu steps, %1.4f mm\n", sa->get_motor_id(), nsteps, nsteps / Robot::getInstance()->actuators[paxis]->get_steps_per_mm());
        }
    }

    // return to where it was
    uint32_t x;
    manual_move(nsteps, sps, sa, !dir, x);

    return true;
}

// do a step by step manual move optionally until the pin is hit
bool Endstops::manual_move(uint32_t steps, uint32_t sps, StepperMotor *sa, bool dir, uint32_t& nsteps, Pin *pin)
{
    // make sure motors are enabled, as manual step does not check
    if(!sa->is_enabled()) sa->enable(true);

    // now move at the required speed until the endstop triggers
    bool hit= (pin == nullptr);
    nsteps= 0; // number of steps actually moved
    uint32_t delayus = 1000000.0F / sps;
    for(uint32_t s = 0; s < steps; s++) {
        if(Module::is_halted()) return false;

        if(pin != nullptr && pin->get()) {
            hit= true;
            break;
        }

        // this won't work as the slave actuator is not registered with the stepticker
        // sa->manual_step(dir);
        // for now we do this hack
        {
            sa->set_direction(dir);
            sa->step();
            // unstep delay (pulse period) set to 4us here
            uint32_t st = benchmark_timer_start();
            while(benchmark_timer_as_us(benchmark_timer_elapsed(st)) < 4);
            sa->unstep();
        }

        ++nsteps;

        // delay
        if(delayus >= 10000) {
            safe_sleep(delayus / 1000);
        } else {
            uint32_t st = benchmark_timer_start();
            while(benchmark_timer_as_us(benchmark_timer_elapsed(st)) < delayus) ;
        }
    }

    return hit;
}
