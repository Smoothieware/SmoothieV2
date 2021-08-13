#pragma once

#include "Module.h"
#include "Pin.h"

class TEMPLATE : public Module {
    public:
        TEMPLATE();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);

    private:
        Pin template_button;
};
