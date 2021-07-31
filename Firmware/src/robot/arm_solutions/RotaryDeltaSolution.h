#pragma once

#include "BaseSolution.h"

class ConfigReader;

class RotaryDeltaSolution : public BaseSolution {
    public:
        RotaryDeltaSolution(ConfigReader&);
        void cartesian_to_actuator(const float[], ActuatorCoordinates &) const override;
        void actuator_to_cartesian(const ActuatorCoordinates &, float[] ) const override;

        bool set_optional(const arm_options_t& options) override;
        bool get_optional(arm_options_t& options, bool force_all) const override;

    private:
        void init();
        int delta_calcAngleYZ(double x0, double y0, double z0, double &theta) const;
        int delta_calcForward(double theta1, double theta2, double theta3, double &x0, double &y0, double &z0) const;

        double delta_e;			// End effector length
        double delta_f;			// Base length
        double delta_re;			// Carbon rod length
        double delta_rf;			// Servo horn length
        double delta_z_offset ;		// Distance from delta 8mm rod/pulley to table/bed
			       		// NOTE: For OpenPnP, set the zero to be about 25mm above the bed

        double delta_ee_offs;		// Ball joint plane to bottom of end effector surface
        double tool_offset;		// Distance between end effector ball joint plane and tip of tool
        double z_calc_offset;

        struct {
            bool debug_flag:1;
            bool mirror_xy:1;
        };
};
