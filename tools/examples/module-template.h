#pragma once

#include "Module.h"
#include "Pin.h"

class GCode;
class OutputStream;

class TEMPLATE : public Module {
    public:
        TEMPLATE();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);

    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        Pin template_button;
};
