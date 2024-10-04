#include "buttonbox.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Pin.h"
#include "OutputStream.h"
#include "Conveyor.h"
#include "MessageQueue.h"

#include <cstring>

#define enable_key "enable"
#define pin_key "pin"
#define press_key "press"
#define release_key "release"

/*
    A button box is a device with many programmable buttons.
    They can be used to define macros or functions or set some state.
    eg
     buttons could be used to jog X and Y axis in each direction.
     A button could issue a command, gcode or mcode.
     A button could set some state used by a subsequent button (like toggle direction)

    TODO handle button matrix scanning
*/
REGISTER_MODULE(ButtonBox, ButtonBox::create)

bool ButtonBox::create(ConfigReader& cr)
{
    printf("DEBUG: configure ButtonBox\n");
    ButtonBox *bb = new ButtonBox();
    if(!bb->configure(cr)) {
        printf("INFO: No buttons enabled\n");
        delete bb;
    }
    return true;
}

ButtonBox::ButtonBox() : Module("buttonbox")
{
}

bool ButtonBox::configure(ConfigReader& cr)
{
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("button box", ssmap)) {
        printf("INFO: configure-buttonbox: no button box section found\n");
        return false;
    }

    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach button
        std::string name = i.first;
        auto& m = i.second;
        if(!cr.get_bool(m, enable_key, true)) continue; // skip if not enabled
        std::string p = cr.get_string(m, pin_key, "nc");
        Pin *b = nullptr;
        bool external = false;
        if(p == "external") {
            external = true;
            b = nullptr;

        } else {
            b = new Pin(p.c_str(), Pin::AS_INPUT);
            if(!b->connected()) {
                printf("ERROR: configure-buttonbox: No, or illegal, button defined\n");
                delete b;
                continue;
            }
            external = false;
        }

        std::string press = cr.get_string(m, press_key, "");
        if(press.empty()) {
            printf("ERROR: configure-buttonbox: No press defined\n");
            if(!external) delete b;
            continue;
        }
        std::string release = cr.get_string(m, release_key, "");

        if(external) {
            but_t bt {
                .name = name,
                .but = nullptr,
                .fnc = nullptr, // must be set by external module
                .press_act = press,
                .release_act = release,
                .state = false,
            };
            buttons.push_back(bt);
            printf("DEBUG: external button %s, press: %s, release: %s\n", name.c_str(), press.c_str(), release.c_str());

        } else {
            but_t bt {
                .name = name,
                .but = b,
                .fnc = nullptr,
                .press_act = press,
                .release_act =  release,
                .state = false,
            };
            buttons.push_back(bt);
            printf("DEBUG: pin button %s - %s, press: %s, release: %s\n", name.c_str(), b->to_string().c_str(), press.c_str(), release.c_str());
        }

        ++cnt;
    }

    printf("INFO: %d button(s) loaded\n", cnt);

    if(cnt > 0) {
        if(SlowTicker::getInstance()->attach(20, std::bind(&ButtonBox::button_tick, this)) < 0) {
            printf("ERROR: configure-buttonbox: failed to attach to SlowTicker\n");
        }
        return true;
    }

    return false;
}

// called by an external module to register the callback to get the named buttons state
bool ButtonBox::set_cb_fnc(const char *name, std::function<bool(const char *name)> fnc)
{
    // find the named button
    for (auto& i : buttons) {
        if(i.name != name) continue;
        if(i.but != nullptr) {
            printf("ERROR: buttonbox set_cb_fnc: The button %s is not an external button\n", name);
            return false;
        }
        i.fnc = fnc;
        return true;
    }

    return false;
}

// Check the state of the buttons and act accordingly
// Note this is the RTOS timer task so don't do anything slow or blocking in here
void ButtonBox::button_tick()
{
    static OutputStream os; // NULL output stream, but we need to keep some state between calls

    for(auto& i : buttons) {
        bool state_changed = false;
        bool new_state;
        const char *cmd = nullptr;

        bool bs;
        if(i.but != nullptr) {
            bs = i.but->get();
        } else {
            // if not defined yet ignore it
            if(i.fnc) {
                bs = i.fnc(i.name.c_str());
            } else {
                continue;
            }
        }

        if(!i.state && bs) {
            // pressed
            cmd = i.press_act.c_str();
            state_changed = true;
            new_state = true;

        } else if(i.state && !bs) {
            // released
            if(!i.release_act.empty()) {
                state_changed = true;
                new_state = false;
                cmd = i.release_act.c_str();
            } else {
                // empty command for release
                i.state = false;
            }
        }

        if(state_changed && cmd != nullptr) {
            if(strcmp(cmd, "$J STOP") == 0) {
                os.set_stop_request(true);
                i.state = new_state;

            } else if(strcmp(cmd, "KILL") == 0) {
                i.state = new_state;
                Module::broadcast_halt(true);

            } else if(strcmp(cmd, "SUSPEND") == 0) {
                i.state = new_state;
                if(is_suspended()) {
                    // resume
                    cmd = "M601";
                } else {
                    // suspend
                    cmd = "M600";
                }
                send_message_queue(cmd, &os, false);

            } else {
                // Do not block and if queue was full the command is not sent
                // so we do not change the state of the button that caused this so it tries again
                // next tick if the button has not changed since.
                if(send_message_queue(cmd, &os, false)) {
                    i.state = new_state;
                }
            }
        }
    }


}

bool ButtonBox::is_suspended() const
{
    auto m = Module::lookup("player");
    if(m != nullptr) {
        bool r = false;
        bool ok = m->request("is_suspended", &r);
        if(ok && r) {
            return true;
        }
    }

    return false;
}
