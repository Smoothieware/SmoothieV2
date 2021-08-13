#pragma once

#include "Module.h"
#include "RingBuffer.h"

class Pin;

class ButtonBox : public Module {
    public:
        ButtonBox();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);
        virtual void in_command_ctx(bool);

    private:
        void button_tick();
        using but_t = struct {
            Pin *but;
            std::string press_act;
            std::string release_act;
            bool state;
        };
        std::vector<but_t> buttons;
        RingBuffer<const char*, 10> commands;
};
