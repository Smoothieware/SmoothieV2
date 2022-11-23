#include "RotaryDeltaSolution.h"

#include "ConfigReader.h"
#include "AxisDefns.h"
#include "Consoles.h"
#include "Module.h"

#include <math.h>

#define delta_e_key "delta_e"
#define delta_f_key "delta_f"
#define delta_re_key "delta_re"
#define delta_rf_key "delta_rf"
#define delta_z_offset_key "delta_z_offset"
#define delta_ee_offs_key "delta_ee_offs"
#define tool_offset_key "delta_tool_offset"
#define delta_mirror_xy_key "delta_mirror_xy"
#define halt_on_error_key "halt_on_error"
#define min_angle_key "min_angle"
#define max_angle_key "max_angle"

constexpr static double pi     = M_PI; // 3.141592653589793238463;    // PI
constexpr static double sin120 = 0.86602540378443864676372317075294; //sqrt3/2.0
constexpr static double cos120 = -0.5;
constexpr static double tan60  = 1.7320508075688772935274463415059; //sqrt3;
constexpr static double sin30  = 0.5;
constexpr static double tan30  = 0.57735026918962576450914878050196; //1/sqrt3

RotaryDeltaSolution::RotaryDeltaSolution(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("rotary delta", m)) {
        printf("WARNING:config-RotaryDeltaSolution: No rotary delta section found\n");
    }

    // End effector length
    delta_e = cr.get_double(m, delta_e_key, 131.636);

    // Base length
    delta_f = cr.get_double(m, delta_f_key, 190.526);

    // Carbon rod length
    delta_re = cr.get_double(m, delta_re_key, 270.000);

    // Servo horn length
    delta_rf = cr.get_double(m, delta_rf_key, 90.000);

    // Distance from delta 8mm rod/pulley to table/bed,
    // NOTE: For OpenPnP, set the zero to be about 25mm above the bed..
    delta_z_offset = cr.get_double(m, delta_z_offset_key, 290.700);

    // Ball joint plane to bottom of end effector surface
    delta_ee_offs = cr.get_double(m, delta_ee_offs_key, 15.000);

    // Distance between end effector ball joint plane and tip of tool (PnP)
    tool_offset = cr.get_double(m, tool_offset_key, 30.500);

    // Minimum and Maximum angle the actuator is allowed to go to before generating an error if halt_on_errors is set
    min_angle = cr.get_float(m, min_angle_key, -45); // all the way down
    max_angle = cr.get_float(m, max_angle_key, 70); // all the way up

    // mirror the XY axis
    mirror_xy = cr.get_bool(m, delta_mirror_xy_key, true);

    halt_on_error= cr.get_bool(m, halt_on_error_key, true);

    debug_flag = false;
    init();
}

// inverse kinematics
// helper functions, calculates angle theta1 (for YZ-pane)
int RotaryDeltaSolution::delta_calcAngleYZ(double x0, double y0, double z0, double &theta) const
{
    double y1 = -0.5 * tan30 * delta_f; // f/2 * tan 30
    y0      -=  0.5 * tan30 * delta_e; // shift center to edge
    // z = a + b*y
    double a = (x0 * x0 + y0 * y0 + z0 * z0 + delta_rf * delta_rf - delta_re * delta_re - y1 * y1) / (2.0 * z0);
    double b = (y1 - y0) / z0;

    double d = -(a + b * y1) * (a + b * y1) + delta_rf * (b * b * delta_rf + delta_rf); // discriminant
    if (d < 0.0) return -1;                                            // non-existing point

    double yj = (y1 - a * b - sqrt(d)) / (b * b + 1.0);               // choosing outer point
    double zj = a + b * yj;

    theta = 180.0 * atan(-zj / (y1 - yj)) / pi + ((yj > y1) ? 180.0 : 0.0);
    return 0;
}

// forward kinematics: (theta1, theta2, theta3) -> (x0, y0, z0)
// returned status: 0=OK, -1=non-existing position
int RotaryDeltaSolution::delta_calcForward(double theta1, double theta2, double theta3, double &x0, double &y0, double &z0) const
{
    double t = (delta_f - delta_e) * tan30 / 2.0;
    double degrees_to_radians = pi / 180.0;

    theta1 *= degrees_to_radians;
    theta2 *= degrees_to_radians;
    theta3 *= degrees_to_radians;

    double y1 = -(t + delta_rf * cos(theta1));
    double z1 = -delta_rf * sin(theta1);

    double y2 = (t + delta_rf * cos(theta2)) * sin30;
    double x2 = y2 * tan60;
    double z2 = -delta_rf * sin(theta2);

    double y3 = (t + delta_rf * cos(theta3)) * sin30;
    double x3 = -y3 * tan60;
    double z3 = -delta_rf * sin(theta3);

    double dnm = (y2 - y1) * x3 - (y3 - y1) * x2;

    double w1 = y1 * y1 + z1 * z1;
    double w2 = x2 * x2 + y2 * y2 + z2 * z2;
    double w3 = x3 * x3 + y3 * y3 + z3 * z3;

    // x = (a1*z + b1)/dnm
    double a1 = (z2 - z1) * (y3 - y1) - (z3 - z1) * (y2 - y1);
    double b1 = -((w2 - w1) * (y3 - y1) - (w3 - w1) * (y2 - y1)) / 2.0;

    // y = (a2*z + b2)/dnm;
    double a2 = -(z2 - z1) * x3 + (z3 - z1) * x2;
    double b2 = ((w2 - w1) * x3 - (w3 - w1) * x2) / 2.0;

    // a*z^2 + b*z + c = 0
    double a = a1 * a1 + a2 * a2 + dnm * dnm;
    double b = 2.0 * (a1 * b1 + a2 * (b2 - y1 * dnm) - z1 * dnm * dnm);
    double c = (b2 - y1 * dnm) * (b2 - y1 * dnm) + b1 * b1 + dnm * dnm * (z1 * z1 - delta_re * delta_re);

    // discriminant
    double d = b * b - 4.0 * a * c;
    if (d < 0.0) return -1; // non-existing point

    z0 = -0.5 * (b + sqrt(d)) / a;
    x0 = (a1 * z0 + b1) / dnm;
    y0 = (a2 * z0 + b2) / dnm;

    z0 -= z_calc_offset;
    return 0;
}


void RotaryDeltaSolution::init()
{
    //these are calculated here and not in the config() as these variables can be fine tuned by the user.
    z_calc_offset  = -(delta_z_offset - tool_offset - delta_ee_offs);
}

static bool is_homed()
{
    Module *m = Module::lookup("endstops");
    if(m != nullptr) {
        bool state;
        bool ok = m->request("is_homed", &state);
        if(ok && state) return true;
    }

    return false;
}

void RotaryDeltaSolution::cartesian_to_actuator(const float cartesian_mm[], ActuatorCoordinates &actuator_mm ) const
{
    //We need to translate the Cartesian coordinates in mm to the actuator position required in mm so the stepper motor  functions
    double alpha_theta = 0.0;
    double beta_theta  = 0.0;
    double gamma_theta = 0.0;

    //Code from Trossen Robotics tutorial, has X in front Y to the right and Z to the left
    // firepick is X at the back and negates X0 X0
    // selected by a config option
    double x0 = cartesian_mm[X_AXIS];
    double y0 = cartesian_mm[Y_AXIS];
    if(mirror_xy) {
        x0 = -x0;
        y0 = -y0;
    }

    double z_with_offset = cartesian_mm[Z_AXIS] + z_calc_offset; //The delta calculation below places zero at the top.  Subtract the Z offset to make zero at the bottom.

    int status =              delta_calcAngleYZ(x0,                    y0,                  z_with_offset, alpha_theta);
    if (status == 0) status = delta_calcAngleYZ(x0 * cos120 + y0 * sin120, y0 * cos120 - x0 * sin120, z_with_offset, beta_theta); // rotate co-ordinates to +120 deg
    if (status == 0) status = delta_calcAngleYZ(x0 * cos120 - y0 * sin120, y0 * cos120 + x0 * sin120, z_with_offset, gamma_theta); // rotate co-ordinates to -120 deg

    if (status == -1) { //something went wrong,
        //force to actuator FPD home position as we know this is a valid position
        // actuator_mm[ALPHA_STEPPER] = 0;
        // actuator_mm[BETA_STEPPER ] = 0;
        // actuator_mm[GAMMA_STEPPER] = 0;


        //DEBUG CODE, uncomment the following to help determine what may be happening if you are trying to adapt this to your own different rotary delta.
        if(debug_flag) {
            printf("//ERROR: Delta calculation fail!  Unable to move to:\n");
            printf("//    x= %f\n", cartesian_mm[X_AXIS]);
            printf("//    y= %f\n", cartesian_mm[Y_AXIS]);
            printf("//    z= %f\n", cartesian_mm[Z_AXIS]);
            printf("// CalcZ= %f\n", z_calc_offset);
            printf("// Offz= %f\n", z_with_offset);
        }

        // we need to HALT here otherwise we may cause damage
        if(halt_on_error) {
            Module::broadcast_halt(true);
            print_to_all_consoles("Error: RotaryDelta illegal move. HALTED\n");
        }

    } else {
        actuator_mm[ALPHA_STEPPER] = alpha_theta;
        actuator_mm[BETA_STEPPER ] = beta_theta;
        actuator_mm[GAMMA_STEPPER] = gamma_theta;

        if(debug_flag) {
            printf("// cartesian x= %f, y= %f, z= %f\n", cartesian_mm[X_AXIS], cartesian_mm[Y_AXIS], cartesian_mm[Z_AXIS]);
            printf("// actuator x= %f, y= %f, z= %f\n", actuator_mm[X_AXIS], actuator_mm[Y_AXIS], actuator_mm[Z_AXIS]);
            printf("// Offz= %f\n", z_with_offset);
        }

        if(halt_on_error && is_homed() && !Module::is_halted()) {
            // check for impossible conditions (like a soft endstop)
            for (int i = ALPHA_STEPPER; i <= GAMMA_STEPPER; ++i) {
                if(actuator_mm[i] < min_angle || actuator_mm[i] > max_angle) {
                    Module::broadcast_halt(true);
                    print_to_all_consoles("Error: rdelta actuator out of range. HALTED\n");
                    break;
                }
            }
        }
    }
}

void RotaryDeltaSolution::actuator_to_cartesian(const ActuatorCoordinates &actuator_mm, float cartesian_mm[] ) const
{
    double x, y, z;
    //Use forward kinematics
    delta_calcForward(actuator_mm[ALPHA_STEPPER], actuator_mm[BETA_STEPPER ], actuator_mm[GAMMA_STEPPER], x, y, z);
    if(mirror_xy) {
        cartesian_mm[X_AXIS] = -x;
        cartesian_mm[Y_AXIS] = -y;
        cartesian_mm[Z_AXIS] = z;
    } else {
        cartesian_mm[X_AXIS] = x;
        cartesian_mm[Y_AXIS] = y;
        cartesian_mm[Z_AXIS] = z;
    }
}

bool RotaryDeltaSolution::set_optional(const arm_options_t &options)
{

    for(auto &i : options) {
        switch(i.first) {
            case 'A': delta_e           = i.second; break;
            case 'B': delta_f           = i.second; break;
            case 'C': delta_re          = i.second; break;
            case 'D': delta_rf          = i.second; break;
            case 'E': delta_z_offset    = i.second; break;
            case 'I': delta_ee_offs     = i.second; break;
            case 'H': tool_offset       = i.second; break;
            case 'W': debug_flag        = i.second != 0; break;
        }
    }
    init();
    return true;
}

bool RotaryDeltaSolution::get_optional(arm_options_t &options, bool force_all) const
{
    options['A'] = this->delta_e;
    options['B'] = this->delta_f;
    options['C'] = this->delta_re;
    options['D'] = this->delta_rf;
    options['E'] = this->delta_z_offset;
    options['I'] = this->delta_ee_offs;
    options['H'] = this->tool_offset;

    return true;
};

