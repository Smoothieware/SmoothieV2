#pragma once

#include "Module.h"

class Lathe;

class ELS : public Module {
    public:
        ELS();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);

    private:
        void check_buttons();
        bool check_button(const char *name);
        void after_load();
        void update_rpm();

        Lathe *lathe{nullptr};
        void *display{nullptr};
        uint8_t buttons{0};
};
