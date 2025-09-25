#include "Robot.h"
#include "Planner.h"
#include "Conveyor.h"
#include "Dispatcher.h"
#include "Pin.h"
#include "StepperMotor.h"
#include "GCode.h"
#include "StepTicker.h"
#include "ConfigReader.h"
#include "StringUtils.h"
#include "main.h"
#include "TemperatureControl.h"
#include "SlowTicker.h"
#include "GCodeProcessor.h"
#include "BaseSolution.h"
#include "CartesianSolution.h"
#include "LinearDeltaSolution.h"
#include "RotaryDeltaSolution.h"
#include "HBotSolution.h"
#include "CoreXZSolution.h"
#include "MorganSCARASolution.h"
#include "Consoles.h"
#include "OutputStream.h"
#include "ActuatorCoordinates.h"

#include <math.h>
#include <string>
#include <algorithm>
#include <tuple>

#define hypotf(a, b) (sqrtf(((a)*(a)) + ((b)*(b))))

#define  default_seek_rate_key          "default_seek_rate"
#define  default_feed_rate_key          "default_feed_rate"
#define  compliant_seek_rate_key        "compliant_seek_rate"
#define  default_acceleration_key       "default_acceleration"
#define  mm_per_line_segment_key        "mm_per_line_segment"
#define  delta_segments_per_second_key  "delta_segments_per_second"
#define  mm_per_arc_segment_key         "mm_per_arc_segment"
#define  mm_max_arc_error_key           "mm_max_arc_error"
#define  arc_correction_key             "arc_correction"
#define  x_axis_max_speed_key           "x_axis_max_speed"
#define  y_axis_max_speed_key           "y_axis_max_speed"
#define  z_axis_max_speed_key           "z_axis_max_speed"
#define  max_speed_key                  "max_speed"
#define  segment_z_moves_key            "segment_z_moves"
#define  save_g92_key                   "save_g92"
#define  set_g92_key                    "set_g92"
#define  nist_G30_key                   "nist_G30"
#define  save_wcs_key                   "save_wcs"
#define  must_be_homed_key              "must_be_homed"

// actuator keys
#define step_pin_key                    "step_pin"
#define dir_pin_key                     "dir_pin"
#define en_pin_key                      "en_pin"
#define steps_per_mm_key                "steps_per_mm"
#define max_rate_key                    "max_rate"
#define acceleration_key                "acceleration"
#define reversed_key                    "reversed"
#define slaved_to_key                   "slaved_to"

// optional pins for microstepping used on smoothiev2 boards
#define ms1_pin_key                     "ms1_pin"
#define ms2_pin_key                     "ms2_pin"
#define ms3_pin_key                     "ms3_pin"
#define ms_key                          "microstepping"
#define microsteps_key                  "microsteps"
#define driver_type_key                 "driver"

// only one of these for all the drivers
#define common_key                      "common"
#define motors_enable_pin_key           "motors_enable_pin"
#define check_driver_errors_key         "check_driver_errors"
#define halt_on_driver_alarm_key        "halt_on_driver_alarm"

// arm solutions
#define  arm_solution_key               "arm_solution"
#define  cartesian_key                  "cartesian"
#define  rostock_key                    "rostock"
#define  linear_delta_key               "linear_delta"
#define  rotary_delta_key               "rotary_delta"
#define  delta_key                      "delta"
#define  hbot_key                       "hbot"
#define  corexy_key                     "corexy"
#define  corexz_key                     "corexz"
#define  kossel_key                     "kossel"
#define  morgan_key                     "morgan"

#define is_grbl_mode() Dispatcher::getInstance()->is_grbl_mode()

#define ARC_ANGULAR_TRAVEL_EPSILON 5E-7F // Float (radians)
#define PI 3.14159265358979323846F // force to be float, do not use M_PI

#define DEFAULT_STEP_PIN(a)  default_stepper_pins[a][0]
#define DEFAULT_DIR_PIN(a)  default_stepper_pins[a][1]
#define DEFAULT_EN_PIN(a)  default_stepper_pins[a][2]

static const char* const default_stepper_pins[][3] = {
#ifdef BOARD_PRIME
    {"PD3", "PD4",  "nc"}, // X step, dir, enb (en must be inverted)
    {"PK2", "PG2",  "nc"}, // Y step, dir, enb
    {"PG3", "PG4",  "nc"}, // Z step, dir, enb
    {"PC6", "PG5",  "nc"}, // A step, dir, enb
#else
    {"nc", "nc",  "nc"}, // X step, dir, enb (en must be inverted)
    {"nc", "nc",  "nc"}, // Y step, dir, enb
    {"nc", "nc",  "nc"}, // Z step, dir, enb
    {"nc", "nc",  "nc"}, // A step, dir, enb
#endif
    {"nc", "nc", "nc"}, // B step, dir, enb
    {"nc", "nc", "nc"}, // C step, dir, enb
};

Robot *Robot::instance = nullptr;

// The Robot converts GCodes into actual movements, and then adds them to the Planner, which passes them to the Conveyor so they can be added to the queue
// It takes care of cutting arcs into segments

Robot::Robot() : Module("robot")
{
    this->inch_mode = false;
    this->absolute_mode = true;
    this->e_absolute_mode = true;
    this->select_plane(X_AXIS, Y_AXIS, Z_AXIS);
    memset(this->machine_position, 0, sizeof machine_position);
    memset(this->compensated_machine_position, 0, sizeof compensated_machine_position);
    memset(this->park_position, 0, sizeof park_position);
    this->park_position[Z_AXIS] = NAN; // optional move if z can home
    for(int i=0;i<3;++i) this->saved_position[i] = NAN;
    this->arm_solution = NULL;
    seconds_per_minute = 60.0F;
    this->clearToolOffset();
    this->compensationTransform = nullptr;
    this->get_e_scale_fnc = nullptr;
    this->wcs_offsets.fill(wcs_t(0.0F, 0.0F, 0.0F));
    this->g92_offset = wcs_t(0.0F, 0.0F, 0.0F);
    this->next_command_is_MCS = false;
    this->disable_segmentation = false;
    this->disable_arm_solution = false;
    this->n_motors = 0;
    this->halt_on_driver_alarm = false;
    this->check_driver_errors = true;
}

// Make keys for the Primary XYZ StepperMotors, and potentially A B C
static const char* const actuator_keys[] = {
    "alpha", // X
    "beta",  // Y
    "gamma", // Z
#if MAX_ROBOT_ACTUATORS > 3
    "delta",   // A
#if MAX_ROBOT_ACTUATORS > 4
    "epsilon", // B
#if MAX_ROBOT_ACTUATORS > 5
    "zeta"     // C
#endif
#endif
#endif
};

static int find_actuator_key(const char *k)
{
    int m = -1;
    for (int i = 0; i < (int)(sizeof(actuator_keys) / sizeof(actuator_keys[0])); ++i) {
        if(strcmp(k, actuator_keys[i]) == 0) {
            m = i;
            break;
        }
    }

    return m;
}

bool Robot::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("motion control", m)) {
        printf("WARNING: configure-robot: no 'motion control' section found, defaults used\n");
    }

    // Arm solutions are used to convert machine positions in millimeters into actuator positions in millimeters.
    // While for a cartesian arm solution, this is a simple multiplication, in other, less simple cases, there is some serious math to be done.
    // To make adding those solution easier, they have their own, separate object.
    // Here we read the config to find out which arm solution to use
    if (this->arm_solution) delete this->arm_solution;

    is_delta = false;
    is_rdelta = false;

    std::string solution = cr.get_string(m, arm_solution_key, "cartesian");

    if(solution == hbot_key || solution == corexy_key) {
        this->arm_solution = new HBotSolution(cr);

    } else if(solution == corexz_key) {
        this->arm_solution = new CoreXZSolution(cr);

    } else if(solution == rostock_key || solution == kossel_key || solution == delta_key || solution ==  linear_delta_key) {
        this->arm_solution = new LinearDeltaSolution(cr);
        is_delta = true;

    } else if(solution == rotary_delta_key) {
        this->arm_solution = new RotaryDeltaSolution(cr);
        is_delta = true;
        is_rdelta = true;

    } else if(solution == morgan_key) {
        this->arm_solution = new MorganSCARASolution(cr);

    } else if(solution == cartesian_key) {
        this->arm_solution = new CartesianSolution(cr);

    } else {
        this->arm_solution = new CartesianSolution(cr);
    }

    this->feed_rate = cr.get_float(m, default_feed_rate_key, 4000.0F); // mm/min
    this->seek_rate = cr.get_float(m, default_seek_rate_key, 4000.0F); // mm/min
    this->compliant_seek_rate = cr.get_bool(m, compliant_seek_rate_key, false);
    this->mm_per_line_segment = cr.get_float(m, mm_per_line_segment_key, 0.0F);
    this->delta_segments_per_second = cr.get_float(m, delta_segments_per_second_key, is_delta ? 100 : 0);
    this->mm_per_arc_segment = cr.get_float(m, mm_per_arc_segment_key, 0.0f);
    this->mm_max_arc_error = cr.get_float(m, mm_max_arc_error_key, 0.01f);
    this->arc_correction = cr.get_float(m, arc_correction_key, 5);

    // in mm/sec but specified in config as mm/min
    this->max_speeds[X_AXIS]  = cr.get_float(m, x_axis_max_speed_key, 60000.0F) / 60.0F;
    this->max_speeds[Y_AXIS]  = cr.get_float(m, y_axis_max_speed_key, 60000.0F) / 60.0F;
    this->max_speeds[Z_AXIS]  = cr.get_float(m, z_axis_max_speed_key, 300.0F) / 60.0F;
    this->max_speed           = cr.get_float(m, max_speed_key, 0) / 60.0F; // zero disables it

    // default acceleration setting, can be overriden with newer per axis settings
    this->default_acceleration = cr.get_float(m, default_acceleration_key, 100.0F); // Acceleration is in mm/s²

    this->segment_z_moves     = cr.get_bool(m, segment_z_moves_key, true);
    this->save_wcs            = cr.get_bool(m, save_wcs_key, false);
    this->save_g92            = cr.get_bool(m, save_g92_key, false);
    this->nist_G30            = cr.get_bool(m, nist_G30_key, false);
    const char *g92           = cr.get_string(m, set_g92_key, "");
    this->must_be_homed       = cr.get_bool(m, must_be_homed_key, is_rdelta || is_delta);

    if(strlen(g92) > 0) {
        // optional setting for a fixed G92 offset
        std::vector<float> t = stringutils::parse_number_list(g92);
        if(t.size() == 3) {
            g92_offset = wcs_t(t[0], t[1], t[2]);
        } else {
            printf("Warning: configure-robot: g92_offset config is bad\n");
        }
    }

    // configure the actuators
    ConfigReader::sub_section_map_t ssm;
    if(!cr.get_sub_sections("actuator", ssm)) {
        printf("ERROR: configure-robot-actuator: no actuator section found\n");
        return false;
    }

    // make each motor
    for (size_t a = 0; a < MAX_ROBOT_ACTUATORS; a++) {
        auto s = ssm.find(actuator_keys[a]);
        if(s == ssm.end()) break; // actuator not found and they must be in contiguous order

        auto& mm = s->second; // map of actuator config values for this actuator
        Pin step_pin(cr.get_string(mm, step_pin_key, DEFAULT_STEP_PIN(a)), Pin::AS_OUTPUT);
        Pin dir_pin( cr.get_string(mm, dir_pin_key,  DEFAULT_DIR_PIN(a)), Pin::AS_OUTPUT);
        Pin en_pin(  cr.get_string(mm, en_pin_key,   DEFAULT_EN_PIN(a)), Pin::AS_OUTPUT);

        // a handy way to reverse motor direction without redefining the pin with !
        bool reversed = cr.get_bool(mm, reversed_key, false);
        if(reversed) {
            if(dir_pin.is_inverting()) {
                printf("WARNING: configure-robot: for actuator %s, the pin is already inverting so reversed is ignored, remove the ! on pin definition and set reversed to false\n", s->first.c_str());
            } else {
                dir_pin.set_inverting(true);
            }
        }

        printf("DEBUG: configure-robot: for actuator %s pins: %s, %s, %s\n", s->first.c_str(), step_pin.to_string().c_str(), dir_pin.to_string().c_str(), en_pin.to_string().c_str());

        if(!step_pin.connected() || !dir_pin.connected()) { // step and dir must be defined, but enable is optional
            if(a <= Z_AXIS) {
                printf("FATAL: configure-robot: motor %c - %s is not defined in config\n", 'X' + a, s->first.c_str());
                n_motors = a; // we only have this number of motors
                return false;
            }
            printf("WARNING: configure-robot: motor %c has no step or dir pin defined\n", 'X' + a);
            break; // if any pin is not defined then the axis is not defined (and axis need to be defined in contiguous order)
        }

        // create the actuator
        StepperMotor *sm = new StepperMotor(step_pin, dir_pin, en_pin);

#if defined(DRIVER_TMC)
        // drivers by default for XYZA are internal, BC are by default external
        // check board ID and select default tmc driver accordingly
        const char *def_driver = board_id == 1 ? "tmc2660" : "tmc2590";
        std::string type = cr.get_string(mm, driver_type_key, a >= 4 ? "external" : def_driver);
        if(type == "tmc2590" || type == "tmc2660") {
            uint32_t t = type == "tmc2590" ? 2590 : 2660;

            // setup the TMC driver for this motor
            if(!sm->setup_tmc(cr, s->first.c_str(), t)) {
                printf("FATAL: configure-robot: setup_tmc%lu failed for %s\n", t, s->first.c_str());
                return false;
            }

            //set microsteps here which will override the raw register setting if any
            uint16_t microstep = cr.get_int(mm, microsteps_key, 32);
            sm->set_microsteps(microstep);
            printf("DEBUG: configure-robot: microsteps for %s set to %d\n", s->first.c_str(), microstep);

            // NOTE at the moment slaved actuators are only implemented for two TMC driver based boardsc
            // if this is a slaved actuator then do not register it as its master will do the setup for it
            // Only A,B,C can be slaved to X,Y,Z, and they must also be the last in the series if there is
            // a real A axis then the slave must be B or C
            std::string slave_to = cr.get_string(mm, slaved_to_key, "");
            if(!slave_to.empty() && slave_to != "none") {
                int st = find_actuator_key(slave_to.c_str());
                // do some sanity checks
                if(st >= A_AXIS) {
                    // Not allowed to slave to axis other than X,Y,Z
                    printf("ERROR: configure-robot: %s slaved to name %s is not allowed\n", s->first.c_str(), slave_to.c_str());
                    break;
                } else if(st < 0) {
                    printf("ERROR: configure-robot: %s slaved to name %s is not found\n", s->first.c_str(), slave_to.c_str());
                    break;
                }

                if(actuators[st]->has_slave()) {
                    printf("ERROR: configure-robot: %s slaved to name %s already has a slave\n", s->first.c_str(), slave_to.c_str());
                    break;
                }

                // we need to give it a motor ID even though it is not in the actuators array
                sm->set_motor_id(a);

                // give the master actuator a pointer to this actuator, and init it
                if(!actuators[st]->init_slave(sm)) {
                    printf("ERROR: configure-robot: %s slaved to name %s had issues see above\n", s->first.c_str(), slave_to.c_str());
                    break;
                }

                printf("INFO: configure-robot: %s (%d) slaved to name %s\n", s->first.c_str(), a, slave_to.c_str());
                continue;
            }

        } else if(type == "external") {
            printf("DEBUG: configure-robot: %s is set as an external driver\n", s->first.c_str());

        } else {
            printf("FATAL: configure-robot: unknown driver type %s\n", type.c_str());
            return false;
        }

#else
        printf("DEBUG: configure-robot: %s is set as an external driver\n", s->first.c_str());
#endif

        // register this actuator (NB This must be 0,1,2,...) of the actuators array
        uint8_t n = register_actuator(sm);
        if(n != a) {
            // this is a fatal error as they must be contiguous
            printf("FATAL: configure-robot: motor %d does not match index %d\n", n, a);
            return false;
        }

        sm->change_steps_per_mm(cr.get_float(mm, steps_per_mm_key, a == Z_AXIS ? 2560.0F : 80.0F));
        sm->set_max_rate(cr.get_float(mm, max_rate_key, 30000.0F) / 60.0F); // it is in mm/min and converted to mm/sec
        sm->set_acceleration(cr.get_float(mm, acceleration_key, -1)); // mm/secs² if -1 it uses the default acceleration
    }

    check_max_actuator_speeds(nullptr); // check the configs are sane

    // common settings for all drivers/actuators
    auto s = ssm.find(common_key);
    if(s != ssm.end()) {
        auto& mm = s->second; // map of general actuator config settings

#if defined(DRIVER_TMC)
        check_driver_errors = cr.get_bool(mm, check_driver_errors_key, true);
        halt_on_driver_alarm = cr.get_bool(mm, halt_on_driver_alarm_key, false);
        const char *default_motor_enn = "PH13!"; // inverted as it is a not enable pin, but we want to set true to enable
#else
        const char *default_motor_enn = "nc";
#endif
        // global enable pin for all motors
        motors_enable_pin = new Pin(cr.get_string(mm, motors_enable_pin_key, default_motor_enn), Pin::AS_OUTPUT_OFF);
        if(!motors_enable_pin->connected()) {
            delete motors_enable_pin;
            motors_enable_pin = nullptr;
            printf("DEBUG: configure-robot: No Motor ENN\n");
        } else {
            motors_enable_pin->set(true); // globally enable motors
            printf("DEBUG: configure-robot: Motor ENN is on pin %s\n", motors_enable_pin->to_string().c_str());
        }
    }

    // initialise actuator positions to current cartesian position (X0 Y0 Z0)
    // so the first move can be correct if homing is not performed
    // Note for deltas this is based on data in config.ini, if overidden in config override it will be wrong
    ActuatorCoordinates actuator_pos;
    arm_solution->cartesian_to_actuator(machine_position, actuator_pos);
    for (size_t i = X_AXIS; i <= Z_AXIS; i++) {
        actuators[i]->change_last_milestone(actuator_pos[i]);
    }

#if MAX_ROBOT_ACTUATORS > 3
    // initialize any extra axis to machine position
    for (size_t i = A_AXIS; i < n_motors; i++) {
        actuators[i]->change_last_milestone(machine_position[i]);
    }
#endif

    //this->clearToolOffset();
#ifdef DRIVER_TMC
    // setup a timer to periodically check VMOT and if it is off we need to tell all motors to reset when it comes on again
    // also will check driver errors and standstill current reduction if enabled
    periodic_checks();
    SlowTicker::getInstance()->attach(1, std::bind(&Robot::periodic_checks, this));
#endif

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    // G Code handlers
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 0, std::bind(&Robot::handle_motion_command, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 1, std::bind(&Robot::handle_motion_command, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 2, std::bind(&Robot::handle_motion_command, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 3, std::bind(&Robot::handle_motion_command, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 4, std::bind(&Robot::handle_dwell, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 10, std::bind(&Robot::handle_G10, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 17, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 18, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 19, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 20, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 21, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 28, std::bind(&Robot::handle_g28_g30, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 30, std::bind(&Robot::handle_g28_g30, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 43, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 49, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 53, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 54, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 55, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 56, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 57, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 58, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 59, std::bind(&Robot::handle_gcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 90, std::bind(&Robot::handle_gcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 91, std::bind(&Robot::handle_gcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 92, std::bind(&Robot::handle_G92, this, _1, _2));


    // M code handlers
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 2, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 3, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 5, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 17, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 18, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 30, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 82, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 83, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 84, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 92, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 114, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 120, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 121, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 203, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 204, std::bind(&Robot::handle_mcodes, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 205, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 220, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 400, std::bind(&Robot::handle_mcodes, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 500, std::bind(&Robot::handle_M500, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 665, std::bind(&Robot::handle_M665, this, _1, _2));
#ifdef DRIVER_TMC
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 909, std::bind(&Robot::handle_M909, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 911, std::bind(&Robot::handle_M911, this, _1, _2));
    THEDISPATCHER->add_handler( "setregs", std::bind( &Robot::handle_setregs_cmd, this, _1, _2) );
#endif
    return true;
}

#ifdef DRIVER_TMC
void Robot::periodic_checks()
{
    // check vmot
    bool vmot;
    float v = get_voltage_monitor("vmotor");
    if(isinf(v) || v < 6) {
        // it is off
        vmot = false;
    } else {
        // it is on
        vmot = true;
    }

    // get last vmot state
    bool last = StepperMotor::set_vmot(vmot);

    // printf("DEBUG: periodic_checks - vmot: %d, last: %d\n", vmot, last);

    if(last && !vmot) {
        // we had a state change to off so set vmot lost
        for(auto a : actuators) {
            a->set_vmot_lost();
        }

        // clear the homed flags as homing is probably lost
        clear_homed();
        printf("DEBUG: vmot went off\n");
    }

    // don't check if we do not have vmot
    if(vmot && check_driver_errors) {
        for(auto a : actuators) {
            if(a->check_driver_error() && halt_on_driver_alarm && !is_halted()) {
                broadcast_halt(true);
                return;
            }
        }
    }
}
#endif

extern Pin *fets_enable_pin; // in main.cpp
extern Pin *fets_power_enable_pin;

// This may be called in a Timer context, but we can send SPI
// flg is true on halt, false on release halt
void Robot::on_halt(bool flg)
{
    halted = flg;

    if(motors_enable_pin != nullptr) {
        // global enable pin for motors, disable on HALT
        motors_enable_pin->set(!flg);
    }
    if(fets_enable_pin != nullptr) {
        // global enable pin for fets, disable fets
        fets_enable_pin->set(!flg);
    }
    if(fets_power_enable_pin != nullptr) {
        // global power enable pin for fets, disable fets power
        fets_power_enable_pin->set(!flg);
    }

    if(flg) {
        for(auto a : actuators) {
            a->enable(false);
            a->stop_moving();
        }
        s_value = 0;
    }
}

void Robot::enable_all_motors(bool flg)
{
    // do not enable motors if in halt state
    if(flg && halted) return;

    if(flg && motors_enable_pin != nullptr) {
        // global motor enable pin
        motors_enable_pin->set(true);
    }
    for(auto a : actuators) {
        a->enable(flg);
    }
}

uint8_t Robot::register_actuator(StepperMotor *motor)
{
    // register this motor with the step ticker
    StepTicker::getInstance()->register_actuator(motor);
    if(n_motors >= k_max_actuators) {
        // this is a fatal error
        printf("FATAL: too many motors, increase k_max_actuators\n");
        return 255;
    }
    actuators.push_back(motor);
    motor->set_motor_id(n_motors);
    return n_motors++;
}

void Robot::push_state()
{
    bool am = this->absolute_mode;
    bool em = this->e_absolute_mode;
    bool im = this->inch_mode;
    saved_state_t s(this->feed_rate, this->seek_rate, am, em, im, current_wcs);
    state_stack.push(s);
}

void Robot::pop_state()
{
    if(!state_stack.empty()) {
        auto s = state_stack.top();
        state_stack.pop();
        this->feed_rate = std::get<0>(s);
        if(!compliant_seek_rate) {
            this->seek_rate = std::get<1>(s);
        }
        this->absolute_mode = std::get<2>(s);
        this->e_absolute_mode = std::get<3>(s);
        this->inch_mode = std::get<4>(s);
        this->current_wcs = std::get<5>(s);
    }
}

std::vector<Robot::wcs_t> Robot::get_wcs_state() const
{
    std::vector<wcs_t> v;
    v.push_back(wcs_t(current_wcs, MAX_WCS, 0));
    for(auto& i : wcs_offsets) {
        v.push_back(i);
    }
    v.push_back(g92_offset);
    v.push_back(tool_offset);
    return v;
}

void Robot::get_current_machine_position(float *pos) const
{
    // get real time current actuator position in mm
    ActuatorCoordinates current_position{
        actuators[X_AXIS]->get_current_position(),
        actuators[Y_AXIS]->get_current_position(),
        actuators[Z_AXIS]->get_current_position()
    };

    // get machine position from the actuator position using FK
    arm_solution->actuator_to_cartesian(current_position, pos);
}

void Robot::print_position(uint8_t subcode, std::string& res, bool ignore_extruders) const
{
    // M114.1 is a new way to do this (similar to how GRBL does it).
    // it returns the realtime position based on the current step position of the actuators.
    // this does require a FK to get a machine position from the actuator position
    // and then invert all the transforms to get a workspace position from machine position
    // M114 just does it the old way uses machine_position and does inverse transforms to get the requested position
    int n = 0;
    char buf[64];
    if(subcode == 0) { // M114 print WCS
        wcs_t pos = mcs2wcs(machine_position);
        n = snprintf(buf, sizeof(buf), "C: X:%1.4f Y:%1.4f Z:%1.4f", from_millimeters(std::get<X_AXIS>(pos)), from_millimeters(std::get<Y_AXIS>(pos)), from_millimeters(std::get<Z_AXIS>(pos)));

    } else if(subcode == 4) {
        // M114.4 print last milestone
        n = snprintf(buf, sizeof(buf), "MP: X:%1.4f Y:%1.4f Z:%1.4f", machine_position[X_AXIS], machine_position[Y_AXIS], machine_position[Z_AXIS]);

    } else if(subcode == 5) {
        // M114.5 print last machine position (which should be the same as M114.1 if axis are not moving and no level compensation)
        // will differ from LMS by the compensation at the current position otherwise
        n = snprintf(buf, sizeof(buf), "CMP: X:%1.4f Y:%1.4f Z:%1.4f", compensated_machine_position[X_AXIS], compensated_machine_position[Y_AXIS], compensated_machine_position[Z_AXIS]);

    } else {
        // get real time positions
        float mpos[3];
        get_current_machine_position(mpos);

        // current_position/mpos includes the compensation transform so we need to get the inverse to get actual position
        if(compensationTransform) compensationTransform(mpos, true); // get inverse compensation transform

        if(subcode == 1) { // M114.1 print realtime WCS
            wcs_t pos = mcs2wcs(mpos);
            n = snprintf(buf, sizeof(buf), "WCS: X:%1.4f Y:%1.4f Z:%1.4f", from_millimeters(std::get<X_AXIS>(pos)), from_millimeters(std::get<Y_AXIS>(pos)), from_millimeters(std::get<Z_AXIS>(pos)));

        } else if(subcode == 2) { // M114.2 print realtime Machine coordinate system
            n = snprintf(buf, sizeof(buf), "MCS: X:%1.4f Y:%1.4f Z:%1.4f", mpos[X_AXIS], mpos[Y_AXIS], mpos[Z_AXIS]);

        } else if(subcode == 3) { // M114.3 print realtime actuator position
            // get real time current actuator position in mm
            ActuatorCoordinates current_position{
                actuators[X_AXIS]->get_current_position(),
                actuators[Y_AXIS]->get_current_position(),
                actuators[Z_AXIS]->get_current_position()
            };
            n = snprintf(buf, sizeof(buf), "APOS: X:%1.4f Y:%1.4f Z:%1.4f", current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS]);
        }
    }

    res.append(buf, n);

#if MAX_ROBOT_ACTUATORS > 3
    // deal with the ABC axis
    for (int i = A_AXIS; i < n_motors; ++i) {
        n = 0;
        if(ignore_extruders && actuators[i]->is_extruder()) continue; // don't show an extruder as that will be E
        if(subcode == 4) { // M114.4 print last milestone
            n = snprintf(buf, sizeof(buf), " %c:%1.4f", 'A' + i - A_AXIS, machine_position[i]);

        } else if(subcode == 2 || subcode == 3) { // M114.2/M114.3 print actuator position which is the same as machine position for ABC
            // current actuator position
            n = snprintf(buf, sizeof(buf), " %c:%1.4f", 'A' + i - A_AXIS, actuators[i]->get_current_position());
        }
        if(n > 0) res.append(buf, n);
    }
#endif
}

// converts current last milestone (machine position without compensation transform) to work coordinate system (inverse transform)
Robot::wcs_t Robot::mcs2wcs(const Robot::wcs_t& pos) const
{
    return std::make_tuple(
               std::get<X_AXIS>(pos) - std::get<X_AXIS>(wcs_offsets[current_wcs]) + std::get<X_AXIS>(g92_offset) - std::get<X_AXIS>(tool_offset),
               std::get<Y_AXIS>(pos) - std::get<Y_AXIS>(wcs_offsets[current_wcs]) + std::get<Y_AXIS>(g92_offset) - std::get<Y_AXIS>(tool_offset),
               std::get<Z_AXIS>(pos) - std::get<Z_AXIS>(wcs_offsets[current_wcs]) + std::get<Z_AXIS>(g92_offset) - std::get<Z_AXIS>(tool_offset)
           );
}

// this does a sanity check that actuator speeds do not exceed steps rate capability
// we will override the actuator max_rate if the combination of max_rate and steps/sec exceeds base_stepping_frequency
void Robot::check_max_actuator_speeds(OutputStream* os)
{
    for (size_t i = 0; i < n_motors; i++) {
        //if(actuators[i]->is_extruder()) continue; //extruders are not included in this check

        float step_freq = actuators[i]->get_max_rate() * actuators[i]->get_steps_per_mm();
        if (step_freq > StepTicker::getInstance()->get_frequency()) {
            actuators[i]->set_max_rate(floorf(StepTicker::getInstance()->get_frequency() / actuators[i]->get_steps_per_mm()));
            if(os == nullptr) {
                printf("WARNING: actuator %d rate exceeds base_stepping_frequency * ..._steps_per_mm: %f, setting to %f\n", i, step_freq, actuators[i]->get_max_rate());
            } else {
                os->printf("WARNING: actuator %d rate exceeds base_stepping_frequency * ..._steps_per_mm: %f, setting to %f\n", i, step_freq, actuators[i]->get_max_rate());
            }
        }
    }
}

bool Robot::handle_dwell(GCode& gcode, OutputStream& os)
{
    // G4 Dwell
    uint32_t delay_ms = 0;
    if (gcode.has_arg('P')) {
        if(is_grbl_mode()) {
            // in grbl mode (and linuxcnc) P is decimal seconds
            float f = gcode.get_arg('P');
            delay_ms = f * 1000.0F;

        } else {
            // in reprap P is milliseconds, they always have to be different!
            delay_ms = gcode.get_int_arg('P');
        }
    }
    if (gcode.has_arg('S')) {
        delay_ms += gcode.get_int_arg('S') * 1000;
    }
    if (delay_ms > 0) {
        // drain queue
        Conveyor::getInstance()->wait_for_idle();
        safe_sleep(delay_ms);
    }

    return true;
}

bool Robot::handle_G10(GCode& gcode, OutputStream& os)
{
    // G10 L2 [L20] Pn Xn Yn Zn set WCS
    if(gcode.has_arg('L') && (gcode.get_int_arg('L') == 2 || gcode.get_int_arg('L') == 20) && gcode.has_arg('P')) {
        size_t n = gcode.get_int_arg('P');
        if(n == 0) n = current_wcs; // set current coordinate system
        else --n;
        if(n < MAX_WCS) {
            float x, y, z;
            std::tie(x, y, z) = wcs_offsets[n];
            if(gcode.get_int_arg('L') == 20) {
                // this makes the current machine position (less compensation transform) the offset
                // get current position in WCS
                wcs_t pos = mcs2wcs(machine_position);

                if(gcode.has_arg('X')) {
                    x -= to_millimeters(gcode.get_arg('X')) - std::get<X_AXIS>(pos);
                }

                if(gcode.has_arg('Y')) {
                    y -= to_millimeters(gcode.get_arg('Y')) - std::get<Y_AXIS>(pos);
                }
                if(gcode.has_arg('Z')) {
                    z -= to_millimeters(gcode.get_arg('Z')) - std::get<Z_AXIS>(pos);
                }

            } else {
                if(absolute_mode) {
                    // the value is the offset from machine zero
                    if(gcode.has_arg('X')) x = to_millimeters(gcode.get_arg('X'));
                    if(gcode.has_arg('Y')) y = to_millimeters(gcode.get_arg('Y'));
                    if(gcode.has_arg('Z')) z = to_millimeters(gcode.get_arg('Z'));
                } else {
                    if(gcode.has_arg('X')) x += to_millimeters(gcode.get_arg('X'));
                    if(gcode.has_arg('Y')) y += to_millimeters(gcode.get_arg('Y'));
                    if(gcode.has_arg('Z')) z += to_millimeters(gcode.get_arg('Z'));
                }
            }
            wcs_offsets[n] = wcs_t(x, y, z);
        }
        return true;
    }

    return false;
}

bool Robot::handle_G92(GCode& gcode, OutputStream& os)
{
    if(gcode.get_subcode() == 1 || gcode.get_subcode() == 2 || gcode.get_num_args() == 0) {
        // reset G92 offsets to 0
        g92_offset = wcs_t(0, 0, 0);

    } else if(gcode.get_subcode() == 3) {
        // initialize G92 to the specified values, only used for saving it with M500
        float x = 0, y = 0, z = 0;
        if(gcode.has_arg('X')) x = gcode.get_arg('X');
        if(gcode.has_arg('Y')) y = gcode.get_arg('Y');
        if(gcode.has_arg('Z')) z = gcode.get_arg('Z');
        g92_offset = wcs_t(x, y, z);

    } else if(gcode.get_subcode() == 4) {
        // do a manual homing based on given coordinates, no endstops required (Old G92 functionality)
        if(gcode.has_arg('X')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('X'), X_AXIS); }
        if(gcode.has_arg('Y')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('Y'), Y_AXIS); }
        if(gcode.has_arg('Z')) { Robot::getInstance()->reset_axis_position(gcode.get_arg('Z'), Z_AXIS); }

    } else {
        // standard setting of the g92 offsets, making current WCS position whatever the coordinate arguments are
        float x, y, z;
        std::tie(x, y, z) = g92_offset;
        // get current position in WCS
        wcs_t pos = mcs2wcs(machine_position);

        // adjust g92 offset to make the current wpos == the value requested
        if(gcode.has_arg('X')) {
            x += to_millimeters(gcode.get_arg('X')) - std::get<X_AXIS>(pos);
        }
        if(gcode.has_arg('Y')) {
            y += to_millimeters(gcode.get_arg('Y')) - std::get<Y_AXIS>(pos);
        }
        if(gcode.has_arg('Z')) {
            z += to_millimeters(gcode.get_arg('Z')) - std::get<Z_AXIS>(pos);
        }
        g92_offset = wcs_t(x, y, z);
    }

#if MAX_ROBOT_ACTUATORS > 3
    if(gcode.get_subcode() == 0 && (gcode.has_arg('E') || gcode.get_num_args() == 0)) {
        // reset the E position, legacy for 3d Printers to be reprap compatible
        // find the selected extruder
        int selected_extruder = get_active_extruder();
        if(selected_extruder > 0) {
            float e = gcode.has_arg('E') ? gcode.get_arg('E') : 0;
            machine_position[selected_extruder] = compensated_machine_position[selected_extruder] = e;
            actuators[selected_extruder]->change_last_milestone(get_e_scale_fnc ? e * get_e_scale_fnc() : e);
        }
    }
    if(gcode.get_subcode() == 0 && gcode.get_num_args() > 0) {
        for (int i = A_AXIS; i < n_motors; i++) {
            // ABC just need to set machine_position and compensated_machine_position if specified
            char axis = 'A' + i - 3;
            if(!actuators[i]->is_extruder() && gcode.has_arg(axis)) {
                float ap = gcode.get_arg(axis);
                machine_position[i] = compensated_machine_position[i] = ap;
                actuators[i]->change_last_milestone(ap); // this updates the last_milestone in the actuator
            }
        }
    }
#endif

    return true;
}

bool Robot::handle_motion_command(GCode& gcode, OutputStream& os)
{

    bool handled = true;
    enum MOTION_MODE_T motion_mode = NONE;
    if( gcode.has_g()) {
        switch( gcode.get_code() ) {
            case 0: motion_mode = SEEK;    break;
            case 1: motion_mode = LINEAR;  break;
            case 2: motion_mode = CW_ARC;  break;
            case 3: motion_mode = CCW_ARC; break;
            default: handled = false; break;
        }
    }

    if( motion_mode != NONE) {
        if(is_must_be_homed()) {
            // FIXME maybe only for axis that can be homed (so an E only move would not trigger this on a delta)
            gcode.set_error("Must be homed before moving");

        } else if((motion_mode == CW_ARC || motion_mode == CCW_ARC) && gcode.has_arg('R')) {
            // we do not support radius mode for arcs
            gcode.set_error("Radius mode not supported by G2 or G3");

        } else {
            is_g123 = motion_mode != SEEK;
            process_move(gcode, motion_mode);
        }

    } else {
        is_g123 = false;
        return false;
    }

    next_command_is_MCS = false; // must be on same line as G0 or G1

    return handled;
}

void Robot::do_park(GCode& gcode, OutputStream& os)
{
    if(is_homed()) {
        if(gcode.get_code() == 30 && (isnan(saved_position[X_AXIS]) || isnan(saved_position[Y_AXIS]))) {
            os.printf("error:cannot move to predefined position as it is not defined (Use G30.1 to define it)\n");
            return;
        }

        // NIST spec says if XYZ are specified rapid move to them first then move to MCS of saved position
        // Smoothie differs from the NIST spec in that all XYZ moves are relative if specified
        // whereas NIST suggests using G91 instead.
        // Smoothie also will move to the Z park position first if it was saved
        push_state();
        float x = 0, y = 0, z = 0;
        if(gcode.has_arg('X')) x = to_millimeters(gcode.get_arg('X'));
        if(gcode.has_arg('Y')) y = to_millimeters(gcode.get_arg('Y'));
        if(gcode.has_arg('Z')) z = to_millimeters(gcode.get_arg('Z'));
        if(x != 0 || y != 0 || z != 0) {
            float deltas[3] = {x, y, z};
            delta_move(deltas, this->seek_rate / 60, 3);
        }
        absolute_mode = true;
        next_command_is_MCS = true; // must use machine coordinates in case G92 or WCS is in effect
        OutputStream nos;
        auto &pos = gcode.get_code() == 28 ? park_position : saved_position;
        if(!isnan(pos[Z_AXIS])) {
            THEDISPATCHER->dispatch(nos, 'G', 0, 'Z', from_millimeters(pos[Z_AXIS]), 0);
        }
        THEDISPATCHER->dispatch(nos, 'G', 0, 'X', from_millimeters(pos[X_AXIS]), 'Y', from_millimeters(pos[Y_AXIS]), 0);

        pop_state();

    } else {
        os.printf("error:cannot move to predefined position as axis are not homed and MCS is unknown\n");
    }
}

bool Robot::handle_g28_g30(GCode& gcode, OutputStream& os)
{
    if(gcode.get_code() == 30 && (!is_grbl_mode() || !nist_G30 || gcode.has_arg('P'))) return false; // keep compatibility unless asked for otherwise

    // Handle the same as G28
    // we only handle the park codes here, the homing module will handle the homing commands
    bool handled = true;
    switch(gcode.get_subcode()) {
        case 0: // G28/G30 in grbl mode will do a rapid to the predefined position otherwise it is home/zprobe command
            if(is_grbl_mode()) {
                do_park(gcode, os);
            } else {
                handled = false;
            }
            break;

        case 1: // G28.1/G30.1 set pre defined position
            if(gcode.has_no_args()) {
                if(is_homed()) {
                    auto &pos = gcode.get_code() == 28 ? park_position : saved_position;
                    // saves current position in absolute machine coordinates
                    get_axis_position(pos, 3); // XYZ are all set
                    if(!can_z_home()) pos[Z_AXIS]= NAN; // cannot use Z if it can't be homed
                    os.printf("position saved, use M500 to make permanent\n");

                } else {
                    os.printf("error:Cannot set predefined position unless axis are homed\n");
                }

            } else {
                // Note the following is only meant to be used for recovering a saved position from config-override
                // or setting Z Park position
                // This is not a standard Gcode
                auto &pos = gcode.get_code() == 28 ? park_position : saved_position;
                if(gcode.has_arg('X')) {
                    pos[X_AXIS] = gcode.get_arg('X');
                } else {
                    os.printf("error:Must supply X\n");
                    return true;
                }
                if(gcode.has_arg('Y')) {
                    pos[Y_AXIS] = gcode.get_arg('Y');
                } else {
                    os.printf("error:Must supply Y\n");
                    return true;
                }

                // only set Z if the Z axis can be homed, otherwise ignore the Z parameter
                if(gcode.has_arg('Z')) {
                    if(can_z_home()) {
                        float z = gcode.get_arg('Z');
                        pos[Z_AXIS] = z;
                    }else{
                        os.printf("warning:cannot save Z as the z axis cannot be homed\n");
                        pos[Z_AXIS] = NAN;
                    }
                }
            }
            break;

        case 2: // G28.2 moves to the predefined position if not in grbl mode
            if(gcode.get_code() == 28 && !is_grbl_mode()) {
                do_park(gcode, os);
            } else {
                handled = false;
            }
            break;

        default: handled = false;
    }

    return handled;
}

// A GCode has been received
// See if the current Gcode line has some orders for us
bool Robot::handle_gcodes(GCode& gcode, OutputStream& os)
{
    bool handled = true;
    if(!gcode.has_g()) return false;

    switch( gcode.get_code() ) {
        case 17: this->select_plane(X_AXIS, Y_AXIS, Z_AXIS);   break;
        case 18: this->select_plane(X_AXIS, Z_AXIS, Y_AXIS);   break;
        case 19: this->select_plane(Y_AXIS, Z_AXIS, X_AXIS);   break;
        case 20: this->inch_mode = true;   break;
        case 21: this->inch_mode = false;   break;
        case 43:
            if(gcode.get_subcode() == 1 || gcode.get_subcode() == 2) {
                float deltas[3] = {0, 0, 0};
                if(gcode.has_arg('X')) deltas[X_AXIS] = to_millimeters(gcode.get_arg('X'));
                if(gcode.has_arg('Y')) deltas[Y_AXIS] = to_millimeters(gcode.get_arg('Y'));
                if(gcode.has_arg('Z')) deltas[Z_AXIS] = to_millimeters(gcode.get_arg('Z'));

                float x, y, z;
                std::tie(x, y, z) = tool_offset;
                if(deltas[X_AXIS] != 0) x += deltas[X_AXIS];
                if(deltas[Y_AXIS] != 0) y += deltas[Y_AXIS];
                if(deltas[Z_AXIS] != 0) z += deltas[Z_AXIS];
                tool_offset = wcs_t(x, y, z);

                if(gcode.get_subcode() == 2) {
                    // we also move
                    delta_move(deltas, this->seek_rate / 60, 3);
                }
            }
            break;

        case 49:
            tool_offset = wcs_t(0, 0, 0);
            break;

        case 53: // G53 not fully supported. G53 G1 X1 Y1 is ok, but G53 X1 Y1 is not supported
            if(gcode.has_no_args()) {
                next_command_is_MCS = true;
            } else {
                os.printf("WARNING: G53 with args is not supported\n");
            }
            break;

        case 54: case 55: case 56: case 57: case 58: case 59:
            // select WCS 0-8: G54..G59, G59.1, G59.2, G59.3
            current_wcs = gcode.get_code() - 54;
            if(gcode.get_code() == 59 && gcode.get_subcode() > 0) {
                current_wcs += gcode.get_subcode();
                if(current_wcs >= MAX_WCS) current_wcs = MAX_WCS - 1;
            }
            break;

        case 90: if(gcode.get_subcode() == 0) {
                this->absolute_mode = true;
                this->e_absolute_mode = true;
            } else return false;
            break;
        case 91: if(gcode.get_subcode() == 0) {
                this->absolute_mode = false;
                this->e_absolute_mode = false;
            } else return false;
            break;
        default: handled = false; break;
    }
    return handled;
}

bool Robot::handle_mcodes(GCode& gcode, OutputStream& os)
{
    bool handled = true;
    if(!gcode.has_m()) return false;

    switch( gcode.get_code() ) {
        // case 0: // M0 feed hold, (M0.1 is release feed hold, except we are in feed hold)
        //     if(is_grbl_mode()) THEKERNEL->set_feed_hold(gcode.get_subcode() == 0);
        //     break;

        case 30: // M30 end of program in grbl mode (otherwise it is delete sdcard file)
            if(!is_grbl_mode()) break;
        // fall through to M2
        case 2: // M2 end of program
            Conveyor::getInstance()->wait_for_idle();
            current_wcs = 0;
            absolute_mode = true;
            seconds_per_minute = 60;
            break;

        case 3: // M3 is spindle on and maybe handled elsewhere, but we want to make the S parameter sticky
            if(gcode.has_arg('S')) {
                set_s_value(gcode.get_arg('S'));
            }
            break;

        case 5: // M5 is spindle off and maybe handled elsewhere, but we want to make the S parameter sticky
            set_s_value(0);
            break;

        case 17:
            enable_all_motors(true); // turn all enable pins on
            break;

        case 18: // this allows individual motors to be turned off, no parameters falls through to turn all off
            if(gcode.get_num_args() > 0) {
                Conveyor::getInstance()->wait_for_idle();

                // motors to turn off
                for (int i = 0; i < n_motors; ++i) {
                    char axis = (i <= Z_AXIS ? 'X' + i : 'A' + (i - 3));
                    if(gcode.has_arg(axis)) actuators[i]->enable(false); // turn it off
                }

                // handle E parameter as currently selected extruder ABC
                if(gcode.has_arg('E')) {
                    // find first selected extruder
                    int i = get_active_extruder();
                    actuators[i]->enable(false);
                }
                break;
            }
        // else fall through to turn all off
        case 84:
            Conveyor::getInstance()->wait_for_idle();
            enable_all_motors(false); // turn all enable pins off
            clear_homed();
            break;

        case 82: e_absolute_mode = true; break;
        case 83: e_absolute_mode = false; break;

        case 92: // M92 - set steps per mm
            for (int i = 0; i < n_motors; ++i) {
                if(actuators[i]->is_extruder()) continue; //extruders handle this themselves
                char axis = (i <= Z_AXIS ? 'X' + i : 'A' + (i - A_AXIS));
                if(gcode.has_arg(axis)) {
                    actuators[i]->change_steps_per_mm(this->to_millimeters(gcode.get_arg(axis)));
                }
                os.printf("%c:%f ", axis, actuators[i]->get_steps_per_mm());
            }
            os.set_append_nl();
            check_max_actuator_speeds(&os);
            return true;

        case 114: {
            std::string buf;
            print_position(gcode.get_subcode(), buf, true); // ignore extruders as they will print E themselves
            os.set_prepend_ok();
            os.set_append_nl();
            os.puts(buf.c_str());
            return true;;
        }

        case 120: // push state
            push_state();
            break;

        case 121: // pop state
            pop_state();
            break;

        case 203: // M203 Set maximum feedrates in mm/sec and max speed, M203.1 set maximum actuator feedrates
            if(gcode.get_num_args() == 0) {
                for (size_t i = X_AXIS; i <= Z_AXIS; i++) {
                    os.printf(" %c:%g ", 'X' + i, gcode.get_subcode() == 0 ? this->max_speeds[i] : actuators[i]->get_max_rate());
                }
                if(gcode.get_subcode() == 1) {
                    for (size_t i = A_AXIS; i < n_motors; i++) {
                        if(actuators[i]->is_extruder()) continue; // extruders handle this themselves
                        os.printf(" %c:%g ", 'A' + i - A_AXIS, actuators[i]->get_max_rate());
                    }

                } else {
                    os.printf(" S:%g P:%g ", this->seek_rate / 60, this->max_speed);
                }

                os.set_append_nl();

            } else {
                for (size_t i = X_AXIS; i <= Z_AXIS; i++) {
                    if (gcode.has_arg('X' + i)) {
                        float v = gcode.get_arg('X' + i);
                        if(gcode.get_subcode() == 0) this->max_speeds[i] = v;
                        else if(gcode.get_subcode() == 1) actuators[i]->set_max_rate(v);
                    }
                }

                if (compliant_seek_rate && gcode.get_subcode() == 0 && gcode.has_arg('S')) {
                    this->seek_rate = gcode.get_arg('S') * 60; // is specified in mm/sec stored in mm/min
                }
                if (gcode.get_subcode() == 0 && gcode.has_arg('P')) {
                    this->max_speed = gcode.get_arg('P'); // is specified in mm/sec
                    if(this->max_speed <= 0.1F) this->max_speed = 0; // disable it
                }

                if(gcode.get_subcode() == 1) {
                    // ABC axis only handle actuator max speeds
                    for (size_t i = A_AXIS; i < n_motors; i++) {
                        if(actuators[i]->is_extruder()) continue; //extruders handle this themselves
                        int c = 'A' + i - A_AXIS;
                        if(gcode.has_arg(c)) {
                            float v = gcode.get_arg(c);
                            actuators[i]->set_max_rate(v);
                        }
                    }
                }

                if(gcode.get_subcode() == 1) check_max_actuator_speeds(&os);
            }
            break;

        case 204: // M204 Snnn - set default acceleration to nnn, Xnnn Ynnn Znnn sets axis specific acceleration
            if (gcode.has_arg('S')) {
                float acc = gcode.get_arg('S'); // mm/s^2
                // enforce minimum
                if (acc < 1.0F) acc = 1.0F;
                this->default_acceleration = acc;
            }
            for (int i = 0; i < n_motors; ++i) {
                if(actuators[i]->is_extruder()) continue; //extruders handle this themselves
                char axis = (i <= Z_AXIS ? 'X' + i : 'A' + (i - A_AXIS));
                if(gcode.has_arg(axis)) {
                    float acc = gcode.get_arg(axis); // mm/s^2
                    // negative or zero disables it
                    if (acc <= 0.0F) acc = -1;
                    actuators[i]->set_acceleration(acc);
                }
            }
            break;

        case 205: // M205 Xnnn - set junction deviation, Z - set Z junction deviation, Snnn - Set minimum planner speed
            if (gcode.has_arg('X')) {
                float jd = gcode.get_arg('X');
                // enforce minimum
                if (jd < 0.0F)
                    jd = 0.0F;
                Planner::getInstance()->xy_junction_deviation = jd;
            }
            if (gcode.has_arg('Z')) {
                float jd = gcode.get_arg('Z');
                // enforce minimum, -1 disables it and uses regular junction deviation
                if (jd <= -1.0F) jd = -1;
                Planner::getInstance()->z_junction_deviation = jd;
            }
            if (gcode.has_arg('S')) {
                float mps = gcode.get_arg('S');
                // enforce minimum
                if (mps < 0.0F)
                    mps = 0.0F;
                Planner::getInstance()->minimum_planner_speed = mps;
            }
            break;

        case 220: // M220 - speed override percentage
            if (gcode.has_arg('S')) {
                float factor = gcode.get_arg('S');
                // enforce minimum 10% speed
                if (factor < 10.0F)
                    factor = 10.0F;
                // enforce maximum 10x speed
                if (factor > 1000.0F)
                    factor = 1000.0F;

                seconds_per_minute = 6000.0F / factor;
            } else {
                os.printf("Speed factor at %6.2f %%\n", 6000.0F / seconds_per_minute);
            }
            break;

        case 400: // wait until all moves are done up to this point
            Conveyor::getInstance()->wait_for_idle();
            break;

        default: handled = false; break;
    }

    return handled;
}

#ifdef DRIVER_TMC
bool Robot::handle_M909(GCode& gcode, OutputStream& os)
{
    for (int i = 0; i < get_number_registered_motors(); i++) {
        char axis = i < 3 ? 'X' + i : 'A' + i - 3;
        if (gcode.has_arg(axis)) {
            uint32_t current_microsteps = actuators[i]->get_microsteps();
            uint32_t microsteps = gcode.get_int_arg(axis);
            actuators[i]->set_microsteps(microsteps);
            os.printf("microsteps for %c temporarily changed to: 1/%d\n", axis, microsteps);
            if(gcode.get_subcode() == 1 && current_microsteps != microsteps) {
                // also reset the steps/mm
                float s = actuators[i]->get_steps_per_mm() * ((float)microsteps / current_microsteps);
                actuators[i]->change_steps_per_mm(s);
                os.printf("steps/mm for %c temporarily changed to: %f\n", axis, s);
                check_max_actuator_speeds(&os);
            }
        }
    }
    return true;
}

/* set or get raw registers
    M911 will dump all the registers and status of all the motors
    M911 Pn will dump the registers and status of the selected motor n

    M911.1 [Pn] display current register settings for preload TMC Configurator

    M911.2 [Pn] Rxxx Vyyy sets Register xxx to value yyy for motor n,
                 xxx == 255 writes the registers
                 xxx == 0 shows what registers are mapped to what

    M911.3 [Pn] will set the options for motor n based on the parameters passed as below, if Pn is not specified it sets for all motors

    M911.3 S0 Unnn Vnnn Wnnn Xnnn Ynnn setConstantOffTimeChopper
              U=constant_off_time, V=blank_time, W=fast_decay_time_setting, X=sine_wave_offset, Y=use_current_comparator
    M911.3 S1 Unnn Vnnn Wnnn Xnnn Ynnn setSpreadCycleChopper
              U=constant_off_time, V=blank_time, W=hysteresis_start, X=hysteresis_end, Y=hysteresis_decrement
    M911.3 S2 Zn setRandomOffTime
              Z=on|off Z1 is on Z0 is off
    M911.3 S3 Zn setDoubleEdge
              Z=on|off Z1 is on Z0 is off
    M911.3 S4 Zn setStepInterpolation
              Z=on|off Z1 is on Z0 is off
    M911.3 S5 Zn setCoolStepEnabled
              Z=on|off Z1 is on Z0 is off
    M911.3 S6 Zn setPassiveFastDecay
              Z=on|off Z1 is on Z0 is off
    M911.3 S7 Onnn Qnnn setStallGuardThreshold
              O=stall_guard_threshold, Q=stall_guard_filter_enabled
    M911.3 S8 Hnnn Innn Jnnn Knnn Lnnn setCoolStepConfiguration
              H=lower_SG_threshold, I=SG_hysteresis, J=current_decrement_step_size, K=current_increment_step_size, L=lower_current_limit
*/
bool Robot::handle_M911(GCode& gcode, OutputStream& os)
{
    if(gcode.get_subcode() <= 1) {
        if(gcode.has_no_args()) {
            // M911 no args dump status for all drivers
            for (int i = 0; i < get_number_registered_motors(); i++) {
                if(gcode.get_subcode() != 1) {
                    if(i == 0 && !StepperMotor::get_vmot()) os.printf("VMOT is off\n");
                    char axis = i < 3 ? 'X' + i : 'A' + i - 3;
                    os.printf("Motor %d (%c)...\n", i, axis);
                    actuators[i]->dump_status(os);
                    os.printf("\n");
                } else {
                    actuators[i]->dump_status(os, false);
                }
            }

        } else if(gcode.has_arg('P')) {
            // M911[.1] Pn dump for specific driver
            int p = gcode.get_int_arg('P');
            if(p < 0 || p >= get_number_registered_motors()) return true;
            actuators[p]->dump_status(os, gcode.get_subcode() != 1);
        }
        return true;

    } else {
        int p = -1;
        if(gcode.has_arg('P')) p = gcode.get_int_arg('P');
        if(p >= get_number_registered_motors()) return true;
        int b, e;
        if(p < 0) {
            b = 0, e = get_number_registered_motors();
        } else {
            b = p; e = p + 1;
        }
        for (p = b; p < e; ++p) {
            if(gcode.get_subcode() == 2 && gcode.has_arg('R') && gcode.has_arg('V')) {
                actuators[p]->set_raw_register(os, gcode.get_int_arg('R'), gcode.get_int_arg('V'));

            } else if(gcode.get_subcode() == 3 ) {
                if(!actuators[p]->set_options(gcode)) {
                    os.printf("No options were recognised\n");
                }
                // else{
                //     os.printf("options were set temporarily for %d\n", p);
                // }
            }
        }
        return true;
    }

    return false;
}

#define HELP(m) if(params == "-h") { os.puts(m); return true; }
bool Robot::handle_setregs_cmd( std::string& params, OutputStream& os )
{
    HELP("setregs 0 00204,981C0,A0000,C000E,E0060\n");
    std::string str = stringutils::shift_parameter( params );
    int m = strtol(str.c_str(), NULL, 10);
    if(m >= get_number_registered_motors()) {
        os.printf("invalid motor number\n");
        return true;
    }

    if(!params.empty()) {
        std::vector<uint32_t> regs = stringutils::parse_number_list(params.c_str(), 16);
        if(regs.size() == 5) {
            uint32_t reg = 0;
            for(auto i : regs) {
                // this just sets the local storage, it does not write to the chip
                actuators[m]->set_raw_register(os, ++reg, i);
            }
            // writes the registers to the chip
            actuators[m]->set_raw_register(os, 255, 0);
            os.printf("registers temporarily set for motor %d\n", m);

        } else {
            os.printf("invalid - 5 registers required\n");
        }
    } else {
        os.printf("invalid no registers\n");
    }

    return true;
}

#endif // ifdef DRIVER_TMC

bool Robot::handle_M500(GCode& gcode, OutputStream& os)
{
    os.printf(";Steps per unit:\nM92 ");
    for (int i = 0; i < n_motors; ++i) {
        if(actuators[i]->is_extruder()) continue; //extruders handle this themselves
        char axis = (i <= Z_AXIS ? 'X' + i : 'A' + (i - A_AXIS));
        os.printf("%c%1.5f ", axis, actuators[i]->get_steps_per_mm());
    }
    os.printf("\n");

    // only print if not 0
    os.printf(";Acceleration mm/sec/sec:\nM204 S%1.5f ", default_acceleration);
    for (int i = 0; i < n_motors; ++i) {
        if(actuators[i]->is_extruder()) continue; // extruders handle this themselves
        char axis = (i <= Z_AXIS ? 'X' + i : 'A' + (i - A_AXIS));
        if(actuators[i]->get_acceleration() > 0) os.printf("%c%1.5f ", axis, actuators[i]->get_acceleration());
    }
    os.printf("\n");

    os.printf(";X- Junction Deviation, Z- Z junction deviation, S - Minimum Planner speed mm/sec:\nM205 X%1.5f Z%1.5f S%1.5f\n", Planner::getInstance()->xy_junction_deviation, Planner::getInstance()->z_junction_deviation, Planner::getInstance()->minimum_planner_speed);

    os.printf(";Max cartesian feedrates in mm/sec, S - Seek rate, P - overall max speed:\nM203 X%1.5f Y%1.5f Z%1.5f S%1.5f", max_speeds[X_AXIS], max_speeds[Y_AXIS], max_speeds[Z_AXIS], seek_rate / 60);
    if(max_speed > 0.1F) {
        os.printf(" P%1.5f\n", max_speed);
    } else {
        os.printf("\n");
    }

    os.printf(";Max actuator feedrates in mm/sec:\nM203.1 ");
    for (int i = 0; i < n_motors; ++i) {
        if(actuators[i]->is_extruder()) continue; // extruders handle this themselves
        char axis = (i <= Z_AXIS ? 'X' + i : 'A' + (i - A_AXIS));
        os.printf("%c%1.5f ", axis, actuators[i]->get_max_rate());
    }
    os.printf("\n");

    // get or save any arm solution specific optional values
    BaseSolution::arm_options_t options;
    if(arm_solution->get_optional(options) && !options.empty()) {
        os.printf(";NOTE it is recommended to change this in config.ini once values are confirmed\n");
        os.printf(";Optional arm solution specific settings:\nM665");
        for(auto &i : options) {
            os.printf(" %c%1.4f", i.first, i.second);
        }
        os.printf("\n");
    }

    if(gcode.get_subcode() == 3) {
        // show temporary settings
        os.printf(";Temporary settings S - delta segs/sec, U - mm/line segment:\nM665 ");
        os.printf("S%1.5f U%1.5f\n", this->delta_segments_per_second, this->mm_per_line_segment);
    }

    // save wcs_offsets and current_wcs
    // TODO this may need to be done whenever they change to be compliant
    if(save_wcs) {
        os.printf(";WCS settings\n");
        os.printf("%s\n", stringutils::wcs2gcode(current_wcs).c_str());
        int n = 1;
        for(auto &i : wcs_offsets) {
            if(i != wcs_t(0, 0, 0)) {
                float x, y, z;
                std::tie(x, y, z) = i;
                os.printf("G10 L2 P%d X%f Y%f Z%f ; %s\n", n, x, y, z, stringutils::wcs2gcode(n - 1).c_str());
            }
            ++n;
        }
    } else {
        os.printf(";WCS settings are not saved\n");
    }

    if(save_g92) {
        // linuxcnc saves G92, so we do too if configured, default is to not save to maintain backward compatibility
        // also it needs to be used to set Z0 on rotary deltas as M206/306 can't be used, so saving it is necessary in that case
        if(g92_offset != wcs_t(0, 0, 0)) {
            float x, y, z;
            std::tie(x, y, z) = g92_offset;
            os.printf("G92.3 X%f Y%f Z%f\n", x, y, z); // sets G92 to the specified values
        }
    }

    if(park_position[X_AXIS] != 0 || park_position[Y_AXIS] != 0 || (!isnan(park_position[Z_AXIS]) && park_position[Z_AXIS] != 0)) {
        os.printf(";predefined park position:\nG28.1 X%1.4f Y%1.4f", park_position[X_AXIS], park_position[Y_AXIS]);
        if(!isnan(park_position[Z_AXIS])) {
            os.printf(" Z%1.4f", park_position[Z_AXIS]);
        }
        os.printf("\n");
    }

    // only save it if both X and Y are set (and optionally Z)
    if(nist_G30 && is_grbl_mode() && !isnan(saved_position[X_AXIS]) && !isnan(saved_position[Y_AXIS])) {
        os.printf(";predefined saved position:\nG30.1 X%1.4f Y%1.4f", saved_position[X_AXIS], saved_position[Y_AXIS]);
        if(!isnan(saved_position[Z_AXIS])) {
            os.printf(" Z%1.4f", saved_position[Z_AXIS]);
        }
        os.printf("\n");
    }

    return true;
}

bool Robot::handle_M665(GCode& gcode, OutputStream& os)
{
    // M665 set optional arm solution variables based on arm solution.
    // the parameter args could be any letter each arm solution only accepts certain ones
    BaseSolution::arm_options_t options = gcode.get_args();
    options.erase('S'); // don't include the S
    options.erase('U'); // don't include the U
    if(options.size() > 0) {
        // set the specified options
        arm_solution->set_optional(options);
    }
    options.clear();
    if(arm_solution->get_optional(options)) {
        // foreach optional value
        for(auto &i : options) {
            // print all current values of supported options
            os.printf("%c: %8.4f ", i.first, i.second);
            os.set_append_nl();
        }
    }

    if(gcode.has_arg('S')) { // set delta segments per second, not saved by M500
        this->delta_segments_per_second = gcode.get_arg('S');
        os.printf("Delta segments set to %8.4f segs/sec\n", this->delta_segments_per_second);

    } else if(gcode.has_arg('U')) { // or set mm_per_line_segment, not saved by M500
        this->mm_per_line_segment = gcode.get_arg('U');
        this->delta_segments_per_second = 0;
        os.printf("mm per line segment set to %8.4f\n", this->mm_per_line_segment);
    }

    return true;
}

int Robot::get_active_extruder() const
{
    for (int i = E_AXIS; i < n_motors; ++i) {
        // find first selected extruder
        if(actuators[i]->is_extruder() && actuators[i]->is_selected()) return i;
    }
    return 0;
}

// process a G0/G1/G2/G3
void Robot::process_move(GCode& gcode, enum MOTION_MODE_T motion_mode)
{
    // we have a G0/G1/G2/G3 so extract parameters and apply offsets to get machine coordinate target
    // get XYZ and one E (which goes to the selected extruder)
    float param[4] {0, 0, 0, 0};
    bool is_param[4] {false, false, false, false};

    // process primary axis
    for(int i = X_AXIS; i <= Z_AXIS; ++i) {
        char letter = 'X' + i;
        if( gcode.has_arg(letter) ) {
            param[i] = this->to_millimeters(gcode.get_arg(letter));
            is_param[i] = true;
        }
    }

    float offset[3] {0, 0, 0};
    for(char letter = 'I'; letter <= 'K'; letter++) {
        if( gcode.has_arg(letter) ) {
            offset[letter - 'I'] = this->to_millimeters(gcode.get_arg(letter));
        }
    }

    // calculate target in machine coordinates (less compensation transform which needs to be done after segmentation)
    float target[n_motors];
    memcpy(target, machine_position, n_motors * sizeof(float));

    if(!next_command_is_MCS) {
        if(this->absolute_mode) {
            // apply wcs offsets and g92 offset and tool offset
            if(is_param[X_AXIS]) {
                target[X_AXIS] = param[X_AXIS] + std::get<X_AXIS>(wcs_offsets[current_wcs]) - std::get<X_AXIS>(g92_offset) + std::get<X_AXIS>(tool_offset);
            }

            if(is_param[Y_AXIS]) {
                target[Y_AXIS] = param[Y_AXIS] + std::get<Y_AXIS>(wcs_offsets[current_wcs]) - std::get<Y_AXIS>(g92_offset) + std::get<Y_AXIS>(tool_offset);
            }

            if(is_param[Z_AXIS]) {
                target[Z_AXIS] = param[Z_AXIS] + std::get<Z_AXIS>(wcs_offsets[current_wcs]) - std::get<Z_AXIS>(g92_offset) + std::get<Z_AXIS>(tool_offset);
            }

        } else {
            // they are deltas from the machine_position if specified
            for(int i = X_AXIS; i <= Z_AXIS; ++i) {
                if(is_param[i]) target[i] = param[i] + machine_position[i];
            }
        }

    } else {
        // already in machine coordinates, we do not add wcs or tool offset for that
        for(int i = X_AXIS; i <= Z_AXIS; ++i) {
            if(is_param[i]) target[i] = param[i];
        }
    }

    float delta_e = 0;

#if MAX_ROBOT_ACTUATORS > 3
    // process extruder parameters, for active extruder only (only one active extruder at a time)
    int selected_extruder = 0;
    if(gcode.has_arg('E')) {
        selected_extruder = get_active_extruder();
        param[E_AXIS] = gcode.get_arg('E');
        is_param[E_AXIS] = true;
    }

    // do E for the selected extruder
    if(selected_extruder > 0 && is_param[E_AXIS]) {
        if(this->e_absolute_mode) {
            target[selected_extruder] = param[E_AXIS];
            delta_e = target[selected_extruder] - machine_position[selected_extruder];
        } else {
            delta_e = param[E_AXIS];
            target[selected_extruder] = delta_e + machine_position[selected_extruder];
        }
    }

    // process ABC axis, this is mutually exclusive to using E for an extruder, so if E is used and A then the results are undefined
    for (int i = A_AXIS; i < n_motors; ++i) {
        char letter = 'A' + i - A_AXIS;
        if(gcode.has_arg(letter)) {
            float p = gcode.get_arg(letter);
            if(this->absolute_mode) {
                target[i] = p;
            } else {
                target[i] = p + machine_position[i];
            }
        }
    }
#endif

    if( gcode.has_arg('F') ) {
        if( motion_mode == SEEK ) {
            if(!compliant_seek_rate) {
                this->seek_rate = this->to_millimeters( gcode.get_arg('F') );
            } else {
                // it seems wrong but F anywhere in compliant code sets the feed rate
                this->feed_rate = this->to_millimeters( gcode.get_arg('F') );
            }

        } else {
            this->feed_rate = this->to_millimeters( gcode.get_arg('F') );
        }
    }

    // S is modal When specified on a G0/1/2/3 command
    if(gcode.has_arg('S')) s_value = gcode.get_arg('S');

    bool moved = false;

    // Perform any physical actions
    switch(motion_mode) {
        case NONE: break;

        case SEEK:
            moved = this->append_line(gcode, target, this->seek_rate / (compliant_seek_rate ? 60 : seconds_per_minute), delta_e );
            break;

        case LINEAR:
            moved = this->append_line(gcode, target, this->feed_rate / seconds_per_minute, delta_e );
            break;

        case CW_ARC:
        case CCW_ARC:
            // Note arcs are not currently supported by extruder based machines, as 3D slicers do not use arcs (G2/G3)
            moved = this->compute_arc(gcode, offset, target, motion_mode);
            break;
    }

    if(moved) {
        // set machine_position to the calculated target
        memcpy(machine_position, target, n_motors * sizeof(float));
    }
}

// reset the machine position for all axis. Used for homing.
// after homing we supply the cartesian coordinates that the head is at when homed,
// however for Z this is the compensated machine position (if enabled)
// So we need to apply the inverse compensation transform to the supplied coordinates to get the correct machine position
// this will make the results from M114 and ? consistent after homing.
// This works for cases where the Z endstop is fixed on the Z actuator and is the same regardless of where XY are.
void Robot::reset_axis_position(float x, float y, float z)
{
    // set both the same initially
    compensated_machine_position[X_AXIS] = machine_position[X_AXIS] = x;
    compensated_machine_position[Y_AXIS] = machine_position[Y_AXIS] = y;
    compensated_machine_position[Z_AXIS] = machine_position[Z_AXIS] = z;

    if(compensationTransform) {
        // apply inverse transform to get machine_position
        compensationTransform(machine_position, true);
    }

    // now set the actuator positions based on the supplied compensated position
    ActuatorCoordinates actuator_pos;
    arm_solution->cartesian_to_actuator(this->compensated_machine_position, actuator_pos);
    for (size_t i = X_AXIS; i <= Z_AXIS; i++) {
        actuators[i]->change_last_milestone(actuator_pos[i]);
    }
}

// Reset the position for an axis (used in homing, and to reset extruder after suspend)
void Robot::reset_axis_position(float position, int axis)
{
    compensated_machine_position[axis] = position;
    if(axis <= Z_AXIS) {
        reset_axis_position(compensated_machine_position[X_AXIS], compensated_machine_position[Y_AXIS], compensated_machine_position[Z_AXIS]);

#if MAX_ROBOT_ACTUATORS > 3
    } else if(axis < n_motors) {
        // ABC and/or extruders need to be set as there is no arm solution for them
        machine_position[axis] = compensated_machine_position[axis] = position;
        actuators[axis]->change_last_milestone(machine_position[axis]);
#endif
    }
}

// similar to reset_axis_position but directly sets the actuator positions in actuators units (eg mm for cartesian, degrees for rotary delta)
// then sets the axis positions to match. currently only called from Endstops.cpp and RotaryDeltaCalibration.cpp
void Robot::reset_actuator_position(const ActuatorCoordinates &ac)
{
    for (size_t i = X_AXIS; i <= Z_AXIS; i++) {
        if(!isnan(ac[i])) actuators[i]->change_last_milestone(ac[i]);
    }

    // now correct axis positions then recorrect actuator to account for rounding
    reset_position_from_current_actuator_position();
}

// Use FK to find out where actuator is and reset to match
// TODO maybe we should only reset axis that are being homed unless this is due to a ON_HALT
void Robot::reset_position_from_current_actuator_position()
{
    ActuatorCoordinates actuator_pos;
    for (size_t i = X_AXIS; i < n_motors; i++) {
        // NOTE actuator::current_position is currently NOT the same as actuator::machine_position after an abrupt abort
        actuator_pos[i] = actuators[i]->get_current_position();
    }

    // discover machine position from where actuators actually are
    arm_solution->actuator_to_cartesian(actuator_pos, compensated_machine_position);
    memcpy(machine_position, compensated_machine_position, sizeof machine_position);

    // compensated_machine_position includes the compensation transform so we need to get the inverse to get actual machine_position
    if(compensationTransform) compensationTransform(machine_position, true); // get inverse compensation transform

    // now reset actuator::machine_position, NOTE this may lose a little precision as FK is not always entirely accurate.
    // NOTE This is required to sync the machine position with the actuator position, we do a somewhat redundant cartesian_to_actuator() call
    // to get everything in perfect sync.
    arm_solution->cartesian_to_actuator(compensated_machine_position, actuator_pos);
    for (size_t i = X_AXIS; i <= Z_AXIS; i++) {
        actuators[i]->change_last_milestone(actuator_pos[i]);
    }

    // Handle extruders and/or ABC axis
#if MAX_ROBOT_ACTUATORS > 3
    for (int i = A_AXIS; i < n_motors; i++) {
        // ABC and/or extruders just need to set machine_position and compensated_machine_position
        float ap = actuator_pos[i];
        if(actuators[i]->is_extruder() && get_e_scale_fnc) ap /= get_e_scale_fnc(); // inverse E scale if there is one and this is an extruder
        machine_position[i] = compensated_machine_position[i] = ap;
        actuators[i]->change_last_milestone(actuator_pos[i]); // this updates the last_milestone in the actuator
    }
#endif
}

// this needs to be done if compensation is turned off for continuous jog
void Robot::reset_compensated_machine_position()
{
    if(compensationTransform) {
        compensationTransform = nullptr;
        // we want to leave it where we have set Z, not where it ended up AFTER compensation so
        // this should correct the Z position to the machine_position
        is_g123 = false; // we don't want the laser to fire
        if(!append_milestone(machine_position, this->seek_rate / 60.0F)) {
            // if it wasn't enough to move then just reset to get everything normalized again
            reset_axis_position(machine_position[X_AXIS], machine_position[Y_AXIS], machine_position[Z_AXIS]);
        }
    }
}

// Convert target (in machine coordinates) to machine_position, then convert to actuator position and append this to the planner
// target is in machine coordinates without the compensation transform, however we save a compensated_machine_position that includes
// all transforms and is what we actually convert to actuator positions
bool Robot::append_milestone(const float target[], float rate_mm_s)
{
    float deltas[n_motors];
    float transformed_target[n_motors]; // adjust target for bed compensation
    float unit_vec[N_PRIMARY_AXIS];

    // unity transform by default
    memcpy(transformed_target, target, n_motors * sizeof(float));

    // check function pointer and call if set to transform the target to compensate for bed
    if(compensationTransform) {
        // some compensation strategies can transform XYZ, some just change Z
        compensationTransform(transformed_target, false);
    }

    bool move = false;
    float sos = 0; // sum of squares for just primary axis (XYZ usually)

    // find distance moved by each axis, use transformed target from the current compensated machine position
    for (size_t i = 0; i < n_motors; i++) {
        deltas[i] = transformed_target[i] - compensated_machine_position[i];
        if(std::abs(deltas[i]) < 0.0001F) continue; // try to ignore float hiccups
        // at least one non zero delta
        move = true;
        if(i < N_PRIMARY_AXIS) {
            sos += powf(deltas[i], 2);
        }
    }

    // nothing moved
    if(!move) return false;

    // see if this is a primary axis move or not
    bool auxilliary_move = true;
    for (int i = 0; i < N_PRIMARY_AXIS; ++i) {
        if(std::abs(deltas[i]) >= 0.0001F) {
            auxilliary_move = false;
            break;
        }
    }

    // total movement, use XYZ if a primary axis otherwise we calculate distance for E after scaling to mm
    float distance = auxilliary_move ? 0 : sqrtf(sos);

    // it is unlikely but we need to protect against divide by zero, so ignore insanely small moves here
    // as the last milestone won't be updated we do not actually lose any moves as they will be accounted for in the next move
    if(!auxilliary_move && distance < 0.00001F) return false;


    if(!auxilliary_move) {
        for (size_t i = X_AXIS; i < N_PRIMARY_AXIS; i++) {
            // find distance unit vector for primary axis only
            unit_vec[i] = deltas[i] / distance;

            // Do not move faster than the configured cartesian limits for XYZ
            if ( i <= Z_AXIS && max_speeds[i] > 0 ) {
                float axis_speed = fabsf(unit_vec[i] * rate_mm_s);

                if (axis_speed > max_speeds[i])
                    rate_mm_s *= ( max_speeds[i] / axis_speed );
            }
        }

        if(this->max_speed > 0.1F && rate_mm_s > this->max_speed) {
            rate_mm_s = this->max_speed;
        }
    }

    // find actuator position given the machine position, use actual adjusted target
    ActuatorCoordinates actuator_pos;
    if(!disable_arm_solution) {
        arm_solution->cartesian_to_actuator( transformed_target, actuator_pos );
        if(is_halted()) return false; // some arm solutions can raise a HALT on a fatal error

    } else {
        // basically the same as cartesian, would be used for special homing situations like for scara
        for (size_t i = X_AXIS; i <= Z_AXIS; i++) {
            actuator_pos[i] = transformed_target[i];
        }
    }

#if MAX_ROBOT_ACTUATORS > 3
    sos = 0;
    int aux_sel = -1; // if only one auxilliary is moving we set this to the the motor id

    for (size_t i = A_AXIS; i < n_motors; i++) {
        actuator_pos[i] = transformed_target[i];
        if(actuators[i]->is_extruder() && get_e_scale_fnc) {
            // for the extruders just copy the position, and possibly scale it from mm³ to mm
            // NOTE this relies on the fact only one extruder is active at a time
            // scale for volumetric or flow rate
            // TODO is this correct? scaling the absolute target? what if the scale changes?
            // for volumetric it basically converts mm³ to mm, but what about flow rate?
            actuator_pos[i] *= get_e_scale_fnc();
        }

        if(auxilliary_move) {
            // for E only moves we need to use the scaled E to calculate the distance
            float d = actuator_pos[i] - actuators[i]->get_last_milestone();
            if(d != 0) {
                sos += powf(d, 2);
                if(aux_sel == -1) {
                    aux_sel = i; // so far the only auxilliary axis that is moving
                } else {
                    aux_sel = 0; // more than one was moving
                }
            }
        }
    }

    if(auxilliary_move) {
        distance = sqrtf(sos); // distance in mm of the e move or of combined ABC moves
        if(distance < 0.00001F) return false;
    }

#endif

    // use default acceleration to start with
    float acceleration = default_acceleration;
    float isecs = rate_mm_s / distance;

    // we need to check we are not exceeding any axis maximum feedrate and/or acceleration
    if(auxilliary_move && aux_sel > 0) {
        // if only one auxilliary axis is moving then we just use its feedrate and acceleration
        // still checking that we are not exceeding its maximum feed rate
        float actuator_rate = (fabsf(actuator_pos[aux_sel] - actuators[aux_sel]->get_last_milestone())) * isecs;
        if (actuator_rate > actuators[aux_sel]->get_max_rate()) {
            rate_mm_s = actuators[aux_sel]->get_max_rate();
        }

        // check acceleration, if the axis sets an acceleration, then use it
        float ma =  actuators[aux_sel]->get_acceleration(); // in mm/sec²
        if(ma > 0.0001F) {  // technically != 0 but allow for IEEE float slop
            acceleration = ma;
        }

    } else {
        // check per-actuator speed limits
        for (size_t actuator = 0; actuator < n_motors; actuator++) {
            float d = fabsf(actuator_pos[actuator] - actuators[actuator]->get_last_milestone());
            if(d == 0 || !actuators[actuator]->is_selected()) continue; // no movement for this actuator

            float actuator_rate = d * isecs;
            if (actuator_rate > actuators[actuator]->get_max_rate()) {
                // derate the feedrate by the amount exceeded
                rate_mm_s *= (actuators[actuator]->get_max_rate() / actuator_rate);
                isecs = rate_mm_s / distance;
            }

            // adjust acceleration to lowest found, for now just primary axis unless it is an auxiliary move
            // TODO we may need to do all of them, check E won't limit XYZ.. it does on long E moves, but not checking it could exceed the E acceleration.
            if(auxilliary_move || actuator < N_PRIMARY_AXIS) {
                float ma =  actuators[actuator]->get_acceleration(); // in mm/sec²
                if(ma > 0.0001F) {  // if axis does not have acceleration set then it uses the default_acceleration
                    float ca = fabsf((d / distance) * acceleration);
                    if (ca > ma) {
                        acceleration *= ( ma / ca );
                    }
                }
            }
        }
    }

    // if we are in feed hold wait here until it is released, this means that even segmented lines will pause
    // TODO implement feed hold
    // while(THEKERNEL->get_feed_hold()) {
    //     safe_sleep(100);
    //     // if we also got a HALT then break out of this
    //     if(halted) return false;
    // }

    // make sure the motors are enabled
    enable_all_motors(true);

    // Append the block to the planner
    // NOTE that distance here should be either the distance travelled by the XYZ axis, or the E mm travel if a solo E move
    // NOTE this call will block until there is room in the block queue
    if(Planner::getInstance()->append_block( actuator_pos, n_motors, rate_mm_s, distance, auxilliary_move ? nullptr : unit_vec, acceleration, s_value, is_g123)) {
        // this is the new compensated machine position
        memcpy(this->compensated_machine_position, transformed_target, n_motors * sizeof(float));
        return true;
    }

    // no actual move
    return false;
}

// Used to plan a single move used by things like endstops when homing, zprobe, extruder firmware retracts etc.
// NOTE doesn't work for long moves on a Delta as the move is not segmented so head will dip into the bowl.
bool Robot::delta_move(const float * delta, float rate_mm_s, uint8_t naxis)
{
    if(halted) return false;

    // catch negative or zero feed rates
    if(rate_mm_s <= 0.0F) {
        return false;
    }

    // get the absolute target position, default is current machine_position
    float target[n_motors];
    memcpy(target, machine_position, n_motors * sizeof(float));

    // add in the deltas to get new target
    for (int i = 0; i < naxis; i++) {
        if(!isnan(delta[i])) target[i] += delta[i];
    }

    // submit for planning and if moved update machine_position
    if(append_milestone(target, rate_mm_s)) {
        memcpy(machine_position, target, n_motors * sizeof(float));
        return true;
    }

    return false;
}

// Append a move to the queue ( cutting it into segments if needed )
bool Robot::append_line(GCode & gcode, const float target[], float rate_mm_s, float delta_e)
{
    // catch negative or zero feed rates and return the same error as GRBL does
    if(rate_mm_s <= 0.0F) {
        gcode.set_error(rate_mm_s == 0 ? "Undefined feed rate" : "feed rate < 0");
        return false;
    }

    // Find out the distance for this move in XYZ in MCS
    float millimeters_of_travel = sqrtf(powf( target[X_AXIS] - machine_position[X_AXIS], 2 ) +  powf( target[Y_AXIS] - machine_position[Y_AXIS], 2 ) +  powf( target[Z_AXIS] - machine_position[Z_AXIS], 2 ));

    if(millimeters_of_travel < 0.00001F) {
        // we have no movement in XYZ, probably E only extrude or retract
        return this->append_milestone(target, rate_mm_s);
    }

    /*
        For extruders, we need to do some extra work to limit the volumetric rate if specified...
        If using volumetric limits we need to be using volumetric extrusion for this to work as Ennn needs to be in mm³ not mm
        We ask Extruder to do all the work but we need to pass in the relevant data.
        NOTE we need to do this before we segment the line (for deltas)
    */
    if(delta_e != 0 && gcode.has_g() && gcode.get_code() == 1) {
        // TODO maybe move this to process_params
        float data[2] = {delta_e, rate_mm_s / millimeters_of_travel};
        // TODO we send this to each extruder which is a waste
        auto mv = Module::lookup_group("extruder");
        if(mv.size() > 0) {
            for(auto m : mv) {
                if(m->request("get_rate", data)) {
                    rate_mm_s *= data[1]; // adjust the feedrate
                    break; // only one can be active the rest will return false
                }
            }
        }
    }

    // We cut the line into smaller segments. This is only needed on a cartesian robot for zgrid, but always necessary for robots with rotational axes like Deltas.
    // In delta robots either mm_per_line_segment can be used OR delta_segments_per_second
    // The latter is more efficient and avoids splitting fast long lines into very small segments, like initial z move to 0, it is what Johanns Marlin delta port does
    uint16_t segments;

    if(this->disable_segmentation || (!segment_z_moves && !gcode.has_arg('X') && !gcode.has_arg('Y'))) {
        segments = 1;

    } else if(this->delta_segments_per_second > 1.0F) {
        // enabled if set to something > 1, it is set to 0.0 by default
        // segment based on current speed and requested segments per second
        // the faster the travel speed the fewer segments needed
        // NOTE rate is mm/sec and we take into account any speed override
        float seconds = millimeters_of_travel / rate_mm_s;
        segments = std::max(1.0F, ceilf(this->delta_segments_per_second * seconds));

    } else {
        if(this->mm_per_line_segment < 0.0001F) {
            segments = 1; // don't segment
        } else {
            segments = ceilf( millimeters_of_travel / this->mm_per_line_segment);
        }
    }

    bool moved = false;
    if (segments > 1) {
        // A vector to keep track of the endpoint of each segment
        float segment_delta[n_motors];
        float segment_end[n_motors];
        memcpy(segment_end, machine_position, n_motors * sizeof(float));

        // How far do we move each segment?
        for (int i = 0; i < n_motors; i++)
            segment_delta[i] = (target[i] - machine_position[i]) / segments;

        // segment 0 is already done - it's the end point of the previous move so we start at segment 1
        // We always add another point after this loop so we stop at segments-1, ie i < segments
        for (int i = 1; i < segments; i++) {
            if(halted) return false; // don't queue any more segments
            for (int j = 0; j < n_motors; j++)
                segment_end[j] += segment_delta[j];

            // Append the end of this segment to the queue
            // this can block waiting for free block queue or if in feed hold
            bool b = this->append_milestone(segment_end, rate_mm_s);
            moved = moved || b;
        }
    }

    // Append the end of this full move to the queue
    if(this->append_milestone(target, rate_mm_s)) moved = true;

    return moved;
}


// Append an arc to the queue ( cutting it into segments as needed )
// TODO does not support any E parameters so cannot be used for 3D printing.
bool Robot::append_arc(GCode &  gcode, const float target[], const float offset[], float radius, bool is_clockwise )
{
    float rate_mm_s = this->feed_rate / seconds_per_minute;
    // catch negative or zero feed rates and return the same error as GRBL does
    if(rate_mm_s <= 0.0F) {
        gcode.set_error(rate_mm_s == 0 ? "Undefined feed rate" : "feed rate < 0");
        return false;
    }

    // Scary math
    float center_axis0 = this->machine_position[this->plane_axis_0] + offset[this->plane_axis_0];
    float center_axis1 = this->machine_position[this->plane_axis_1] + offset[this->plane_axis_1];
    float linear_travel = target[this->plane_axis_2] - this->machine_position[this->plane_axis_2];
    float r_axis0 = -offset[this->plane_axis_0]; // Radius vector from center to current location
    float r_axis1 = -offset[this->plane_axis_1];
    float rt_axis0 = target[this->plane_axis_0] - center_axis0;
    float rt_axis1 = target[this->plane_axis_1] - center_axis1;

    // initialize linear travel for ABC
#if MAX_ROBOT_ACTUATORS > 3
    float abc_travel[n_motors - 3];
    for (int i = A_AXIS; i < n_motors; i++) {
        abc_travel[i - 3] = target[i] - this->machine_position[i];
    }
#endif

    // Patch from GRBL Firmware - Christoph Baumann 04072015
    // CCW angle between position and target from circle center. Only one atan2() trig computation required.
    float angular_travel = atan2f(r_axis0 * rt_axis1 - r_axis1 * rt_axis0, r_axis0 * rt_axis0 + r_axis1 * rt_axis1);
    if (is_clockwise) { // Correct atan2 output per direction
        if (angular_travel >= -ARC_ANGULAR_TRAVEL_EPSILON) { angular_travel -= (2 * PI); }
    } else {
        if (angular_travel <= ARC_ANGULAR_TRAVEL_EPSILON) { angular_travel += (2 * PI); }
    }

    // Find the distance for this gcode
    float millimeters_of_travel = hypotf(angular_travel * radius, fabsf(linear_travel));

    // We don't care about non-XYZ moves ( for example the extruder produces some of those )
    if( millimeters_of_travel < 0.00001F ) {
        return false;
    }

    // limit segments by maximum arc error
    float arc_segment = this->mm_per_arc_segment;
    if ((this->mm_max_arc_error > 0) && (2 * radius > this->mm_max_arc_error)) {
        float min_err_segment = 2 * sqrtf((this->mm_max_arc_error * (2 * radius - this->mm_max_arc_error)));
        if (this->mm_per_arc_segment < min_err_segment) {
            arc_segment = min_err_segment;
        }
    }
    // Figure out how many segments for this gcode
    // TODO for deltas we need to make sure we are at least as many segments as requested, also if mm_per_line_segment is set we need to use the
    uint16_t segments = ceilf(millimeters_of_travel / arc_segment);

    //printf("Radius %f - Segment Length %f - Number of Segments %d\n",radius,arc_segment,segments);  // Testing Purposes ONLY
    float theta_per_segment = angular_travel / segments;
    float linear_per_segment = linear_travel / segments;
#if MAX_ROBOT_ACTUATORS > 3
    float abc_per_segment[n_motors - 3];
    for (int i = 0; i < n_motors - 3; i++) {
        abc_per_segment[i] = abc_travel[i] / segments;
    }
#endif

    /* Vector rotation by transformation matrix: r is the original vector, r_T is the rotated vector,
    and phi is the angle of rotation. Based on the solution approach by Jens Geisler.
    r_T = [cos(phi) -sin(phi);
    sin(phi) cos(phi] * r ;
    For arc generation, the center of the circle is the axis of rotation and the radius vector is
    defined from the circle center to the initial position. Each line segment is formed by successive
    vector rotations. This requires only two cos() and sin() computations to form the rotation
    matrix for the duration of the entire arc. Error may accumulate from numerical round-off, since
    all float numbers are single precision on the Arduino. (True float precision will not have
    round off issues for CNC applications.) Single precision error can accumulate to be greater than
    tool precision in some cases. Therefore, arc path correction is implemented.

    Small angle approximation may be used to reduce computation overhead further. This approximation
    holds for everything, but very small circles and large mm_per_arc_segment values. In other words,
    theta_per_segment would need to be greater than 0.1 rad and N_ARC_CORRECTION would need to be large
    to cause an appreciable drift error. N_ARC_CORRECTION~=25 is more than small enough to correct for
    numerical drift error. N_ARC_CORRECTION may be on the order a hundred(s) before error becomes an
    issue for CNC machines with the single precision Arduino calculations.
    This approximation also allows mc_arc to immediately insert a line segment into the planner
    without the initial overhead of computing cos() or sin(). By the time the arc needs to be applied
    a correction, the planner should have caught up to the lag caused by the initial mc_arc overhead.
    This is important when there are successive arc motions.
    */
    // Vector rotation matrix values
    float cos_T = 1 - 0.5F * theta_per_segment * theta_per_segment; // Small angle approximation
    float sin_T = theta_per_segment;

    float arc_target[n_motors];
    float sin_Ti;
    float cos_Ti;
    float r_axisi;
    uint16_t i;
    int8_t count = 0;

    // init array for all axis
    memcpy(arc_target, machine_position, n_motors * sizeof(float));

    // Initialize the linear axis
    arc_target[this->plane_axis_2] = this->machine_position[this->plane_axis_2];

    bool moved = false;
    for (i = 1; i < segments; i++) { // Increment (segments-1)
        if(halted) return false; // don't queue any more segments

        if (count < this->arc_correction ) {
            // Apply vector rotation matrix
            r_axisi = r_axis0 * sin_T + r_axis1 * cos_T;
            r_axis0 = r_axis0 * cos_T - r_axis1 * sin_T;
            r_axis1 = r_axisi;
            count++;
        } else {
            // Arc correction to radius vector. Computed only every N_ARC_CORRECTION increments.
            // Compute exact location by applying transformation matrix from initial radius vector(=-offset).
            cos_Ti = cosf(i * theta_per_segment);
            sin_Ti = sinf(i * theta_per_segment);
            r_axis0 = -offset[this->plane_axis_0] * cos_Ti + offset[this->plane_axis_1] * sin_Ti;
            r_axis1 = -offset[this->plane_axis_0] * sin_Ti - offset[this->plane_axis_1] * cos_Ti;
            count = 0;
        }

        // Update arc_target location
        arc_target[this->plane_axis_0] = center_axis0 + r_axis0;
        arc_target[this->plane_axis_1] = center_axis1 + r_axis1;
        arc_target[this->plane_axis_2] += linear_per_segment;
#if MAX_ROBOT_ACTUATORS > 3
        for (int a = A_AXIS; a < n_motors; a++) {
            arc_target[a] += abc_per_segment[a - 3];
        }
#endif
        // Append this segment to the queue
        bool b = this->append_milestone(arc_target, rate_mm_s);
        moved = moved || b;
    }

    // Ensure last segment arrives at target location.
    if(this->append_milestone(target, rate_mm_s)) moved = true;

    return moved;
}

// Do the math for an arc and add it to the queue
bool Robot::compute_arc(GCode &  gcode, const float offset[], const float target[], enum MOTION_MODE_T motion_mode)
{

    // Find the radius
    float radius = hypotf(offset[this->plane_axis_0], offset[this->plane_axis_1]);

    // Set clockwise/counter-clockwise sign for mc_arc computations
    bool is_clockwise = false;
    if( motion_mode == CW_ARC ) {
        is_clockwise = true;
    }

    // Append arc
    return this->append_arc(gcode, target, offset,  radius, is_clockwise );
}

void Robot::select_plane(uint8_t axis_0, uint8_t axis_1, uint8_t axis_2)
{
    this->plane_axis_0 = axis_0;
    this->plane_axis_1 = axis_1;
    this->plane_axis_2 = axis_2;
}

void Robot::clearToolOffset()
{
    this->tool_offset = wcs_t(0, 0, 0);
}

void Robot::setToolOffset(const float offset[3])
{
    this->tool_offset = wcs_t(offset[0], offset[1], offset[2]);
}

float Robot::get_feed_rate(int code) const
{
    if(code == -1) {
        // modal is currently in gcode processor which is a static in main.cpp
        return GCodeProcessor::get_group1_modal_code() == 0 ? seek_rate : feed_rate;
    } else {
        return code == 0 ? seek_rate : feed_rate;
    }
}

// return a GRBL-like query string for ? command
void Robot::get_query_string(std::string & str) const
{
    bool homing = false;
    bool running = false;
    bool feed_hold = false;

    // see if we are homing
    Module *m = Module::lookup("endstops");
    if(m != nullptr) {
        bool state;
        bool ok = m->request("get_homing_status", &state);
        if(ok && state) homing = true;
    }

    str.append("<");
    if(halted) {
        str.append("Alarm");
    } else if(homing) {
        running = true;
        str.append("Home");
    } else if(feed_hold) {
        str.append("Hold");
    } else if(Conveyor::getInstance()->is_idle()) {
        str.append("Idle");
    } else {
        running = true;
        str.append("Run");
    }

    if(running) {
        float mpos[3];
        get_current_machine_position(mpos);
        // current_position/mpos includes the compensation transform so we need to get the inverse to get actual position
        if(compensationTransform) compensationTransform(mpos, true); // get inverse compensation transform

        char buf[128];
        // machine position
        size_t n = snprintf(buf, sizeof(buf), "%1.4f,%1.4f,%1.4f", from_millimeters(mpos[0]), from_millimeters(mpos[1]), from_millimeters(mpos[2]));

        str.append("|MPos:").append(buf, n);

#if MAX_ROBOT_ACTUATORS > 3
        // deal with the ABC axis (E will be A)
        for (int i = A_AXIS; i < get_number_registered_motors(); ++i) {
            // current actuator position
            n = snprintf(buf, sizeof(buf), ",%1.4f", actuators[i]->get_current_position());
            str.append(buf, n);
        }
#endif

        // work space position
        Robot::wcs_t pos = mcs2wcs(mpos);
        n = snprintf(buf, sizeof(buf), "%1.4f,%1.4f,%1.4f", from_millimeters(std::get<X_AXIS>(pos)), from_millimeters(std::get<Y_AXIS>(pos)), from_millimeters(std::get<Z_AXIS>(pos)));
        str.append("|WPos:").append(buf, n);

        // current feedrate
        float fr = from_millimeters(Conveyor::getInstance()->get_current_feedrate() * 60.0F);
        float frr = from_millimeters(get_feed_rate());
        float fro = 6000.0F / get_seconds_per_minute();
        n = snprintf(buf, sizeof(buf), "|F:%1.1f,%1.1f,%1.1f", fr, frr, fro);
        if(n > sizeof(buf)) n = sizeof(buf);
        str.append(buf, n);

        // S value (either laser or spindle)
        float sr = get_s_value();
        n = snprintf(buf, sizeof(buf), "|S:%1.4f", sr);
        str.append(buf, n);

        m = Module::lookup("laser");
        if(m != nullptr) {
            // current Laser power if laser module is active
            float lp;
            bool ok = m->request("get_current_power", &lp);
            if(ok) {
                n = snprintf(buf, sizeof(buf), "|L:%1.4f", lp);
                str.append(buf, n);
            }
        }

    } else {
        // return the last milestone if idle
        // uses machine position
        char buf[128];
        size_t n = snprintf(buf, sizeof(buf), "%1.4f,%1.4f,%1.4f", from_millimeters(machine_position[X_AXIS]), from_millimeters(machine_position[Y_AXIS]), from_millimeters(machine_position[Z_AXIS]));
        if(n > sizeof(buf)) n = sizeof(buf);

        str.append("|MPos:").append(buf, n);

#if MAX_ROBOT_ACTUATORS > 3
        // deal with the ABC axis (E will be A)
        for (int i = A_AXIS; i < n_motors; ++i) {
            // machine position
            n = snprintf(buf, sizeof(buf), ",%1.4f", machine_position[i]);
            if(n > sizeof(buf)) n = sizeof(buf);
            str.append(buf, n);
        }
#endif

        // work space position
        Robot::wcs_t pos = mcs2wcs(machine_position);
        n = snprintf(buf, sizeof(buf), "%1.4f,%1.4f,%1.4f", from_millimeters(std::get<X_AXIS>(pos)), from_millimeters(std::get<Y_AXIS>(pos)), from_millimeters(std::get<Z_AXIS>(pos)));
        str.append("|WPos:").append(buf, n);

        // requested framerate, and override
        float fr = from_millimeters(get_feed_rate());
        float fro = 6000.0F / get_seconds_per_minute();
        n = snprintf(buf, sizeof(buf), "|F:%1.1f,%1.1f", fr, fro);
        if(n > sizeof(buf)) n = sizeof(buf);
        str.append(buf, n);

        m = Module::lookup("laser");
        if(m == nullptr) {
            // S is spindle RPM
            float sr = get_s_value();
            n = snprintf(buf, sizeof(buf), "|S:%1.2f", sr);
            str.append(buf, n);
        }
    }

    // if not grbl mode get temperatures
    if(!is_grbl_mode()) {
        std::vector<Module*> controllers = Module::lookup_group("temperature control");
        for(auto c : controllers) {
            TemperatureControl::pad_temperature_t temp;
            if(c->request("get_current_temperature", &temp)) {
                char buf[32];
                size_t n = snprintf(buf, sizeof(buf), "|%s:%3.1f,%3.1f", temp.designator.c_str(), temp.current_temperature, temp.target_temperature);
                if(n > sizeof(buf)) n = sizeof(buf);
                str.append(buf, n);
            }
        }
    }

    // See if playing from SD Card and get progress if so
    m = Module::lookup("player");
    if(m != nullptr) {
        std::tuple<bool, unsigned long, unsigned char> r;
        bool ok = m->request("is_playing", &r);
        if(ok && std::get<0>(r)) {
            char buf[32];
            size_t n = snprintf(buf, sizeof(buf), "|SD:%lu,%u", std::get<1>(r), std::get<2>(r));
            if(n > sizeof(buf)) n = sizeof(buf);
            str.append(buf, n);
        }
    }

    str.append(">\n");
}

bool Robot::is_homed() const
{
    Module *m = Module::lookup("endstops");
    if(m != nullptr) {
        bool state;
        bool ok = m->request("is_homed", &state);
        if(ok && state) return true;
    }

    return false;
}

bool Robot::can_z_home() const
{
    Module *m = Module::lookup("endstops");
    if(m != nullptr) {
        bool state;
        bool ok = m->request("can_z_home", &state);
        if(ok && state) return true;
    }

    return false;
}

void Robot::clear_homed()
{
    Module *m = Module::lookup("endstops");
    if(m != nullptr) {
        m->request("clear_homed", nullptr);
    }
}

bool Robot::is_must_be_homed() const
{
    if(must_be_homed && !is_homed()) {
        return true;
    }

    return false;
}
