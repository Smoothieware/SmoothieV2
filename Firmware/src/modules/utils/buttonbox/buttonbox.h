#pragma once

#include "Module.h"

#include <functional>

class Pin;

class ButtonBox : public Module {
    public:
        ButtonBox();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);
        bool set_cb_fnc(const char *name, std::function<bool(const char *name)> fnc);

    private:
        void button_tick();
        using but_t = struct {
            std::string name;
            Pin *but;
            std::function<bool(const char *name)> fnc;
            std::string press_act;
            std::string release_act;
            bool state;
        };
        std::vector<but_t> buttons;
};
