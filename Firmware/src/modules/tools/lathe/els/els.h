#pragma once

#include "Module.h"

class Lathe;
class TM1638;

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

        uint32_t var1{0};
        float rpm;
        Lathe *lathe{nullptr};
        TM1638 *tm{nullptr};
        uint8_t buttons{0};
        bool started{false};
};
