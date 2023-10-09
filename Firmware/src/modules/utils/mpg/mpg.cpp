#include "mpg.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "Consoles.h"
#include "Dispatcher.h"
#include "GCode.h"
#include "RotaryEncoder.h"
#include "Pin.h"
#include "OutputStream.h"
#include "Robot.h"
#include "StepperMotor.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define enable_key "enable"
#define enca_pin_key "enca_pin"
#define encb_pin_key "encb_pin"
#define axis_key "axis"

REGISTER_MODULE(MPG, MPG::create)

bool MPG::create(ConfigReader& cr)
{
    printf("DEBUG: configure MPG(s)\n");

    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("mpg", ssmap)) {
        printf("INFO: configure-mpg: no mpg section found\n");
        return false;
    }

    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach mpg
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, enable_key, false)) {
            MPG *t = new MPG();
            if(t->configure(cr, m, name.c_str())) {
                ++cnt;
            }else{
                printf("WARNING: failed to configure MPG %s\n", name.c_str());
                delete t;
            }
        }
    }

    printf("INFO: %d mpg(s) loaded\n", cnt);
    return cnt > 0;
}

MPG::MPG() : Module("mpg")
{
}

bool MPG::configure(ConfigReader& cr, ConfigReader::section_map_t& m, const char *name)
{
    int a = cr.get_int(m, axis_key, -1);
    if(a < 0 || a > 5) {
        printf("ERROR: configure-mpg %s: axis must be configured and be 0-5\n", name);
        return false;
    }

    axis = a;

    // pin1 and pin2 must be interrupt capable pins that have not already got interrupts assigned for that line number
    Pin *pin1, *pin2;
    pin1 = new Pin(cr.get_string(m, enca_pin_key , "nc"));
    pin2 = new Pin(cr.get_string(m, encb_pin_key , "nc"));

    enc = new RotaryEncoder(*pin1, *pin2, std::bind(&MPG::handle_change, this));
    if(!enc->setup()) {
        printf("ERROR: configure-mpg %s: enca and/or encb pins are not valid interrupt pins\n", name);
        delete pin1;
        delete pin2;
        delete enc;
        return false;
    }

    // setup task to handle encoder changes
    xBinarySemaphore = xSemaphoreCreateBinary();
    xTaskCreate(vHandlerTask, "EncoderHandler", 512, this, 3, NULL);

    return true;
}

void MPG::vHandlerTask(void *instance)
{
    MPG *i = static_cast<MPG*>(instance);
    i->check_encoder();
}

void MPG::check_encoder()
{
    while(1) {
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        uint32_t c = enc->get_count();
        // TODO handle wrap around
        if(c != last_count) {
            int delta = c - last_count;
            bool dir = c > last_count;
            last_count = c;
            for (int i = 0; i < std::abs(delta); ++i) {
                Robot::getInstance()->actuators[axis]->manual_step(dir);
            }
        }
    }
}

void MPG::handle_change()
{
    // the encoder was rotated, signal that the movement thread should run
    xSemaphoreGiveFromISR(xBinarySemaphore, pdFALSE);
}


/*
    example config.ini entry:-

    [mpg]
    xaxis.enable = true
    xaxis.enca_pin = PA1^
    xaxis.encb_pin = PA2^
    xaxis.axis = 0

    yaxis.enable = true
    yaxis.enca_pin = PA3^
    yaxis.encb_pin = PA4^
    yaxis.axis = 1
*/
