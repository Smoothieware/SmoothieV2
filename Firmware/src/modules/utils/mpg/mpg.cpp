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

SemaphoreHandle_t MPG::xBinarySemaphore;
MPG *MPG::instance;

REGISTER_MODULE(MPG, MPG::create)

bool MPG::create(ConfigReader& cr)
{
    printf("DEBUG: configure MPG\n");
    MPG *t = new MPG();
    if(!t->configure(cr)) {
        printf("INFO: No MPG enabled\n");
        delete t;
    }

    return true;
}

MPG::MPG() : Module("mpg")
{
    instance = this;
}

bool MPG::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("mpg", m)) return false;

    bool enable = cr.get_bool(m,  enable_key , false);
    if(!enable) {
        return false;
    }

    int a = cr.get_int(m, axis_key, -1);
    if(a < 0 || a > 5) {
        printf("ERROR: configure-mpg: axis must be configured and be 0-5\n");
        return false;
    }

    axis = a;

    // pin1 and pin2 must be interrupt capable pins that have not already got interrupts assigned for that line number
    Pin *pin1, *pin2;
    pin1 = new Pin(cr.get_string(m, enca_pin_key , "nc"));

    if(!pin1->connected()) {
        printf("ERROR: configure-mpg: enca pin is invalid\n");
        delete pin1;
        return false;
    }

    pin2 = new Pin(cr.get_string(m, encb_pin_key , "nc"));
    if(!pin2->connected()) {
        printf("ERROR: configure-mpg: encb pin is invalid\n");
        delete pin1;
        delete pin2;
        return false;
    }

    enc = new RotaryEncoder(*pin1, *pin2, std::bind(&MPG::handle_change, this));
    if(!enc->setup()) {
        printf("ERROR: configure-mpg: enca and/or encb pins are not valid interrupt pins\n");
        delete pin1;
        delete pin2;
        delete enc;
        return false;
    }

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    // command handlers
    THEDISPATCHER->add_handler("mpg", std::bind(&MPG::handle_cmd, this, _1, _2));

    // setup task to handle encoder changes
    xBinarySemaphore = xSemaphoreCreateBinary();
    xTaskCreate(vHandlerTask, "EncoderHandler", 512, NULL, 3, NULL);

    return true;
}

void MPG::vHandlerTask(void *pvParameters)
{
    instance->check_encoder();
}

void MPG::check_encoder()
{
    while(1) {
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        uint32_t c = enc->get_count();
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

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }
bool MPG::handle_cmd(std::string& params, OutputStream& os)
{
    HELP("mpg [on|off] - enable the mpg encoder rotary dial to jog");
    enabled = (params == "on");
    if(enabled) {
        enc->reset_count();
        last_count = 0;
    }
    return true;
}

void MPG::handle_change()
{
    // the encoder was rotated, signal that the movement thread should run
    if(enabled) {
        xSemaphoreGiveFromISR(xBinarySemaphore, pdFALSE);
    }
}
