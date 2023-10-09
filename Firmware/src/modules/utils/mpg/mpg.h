#pragma once

#include "Module.h"
#include "Pin.h"

#include <string>

class GCode;
class OutputStream;
class RotaryEncoder;

class MPG : public Module {
    public:
        MPG();
        static bool create(ConfigReader& cr);

    private:
        bool configure(ConfigReader& cr);
        bool handle_cmd(std::string& params, OutputStream& os);
        void handle_change();
        static void vHandlerTask(void *pvParameters);
        void check_encoder();

        uint8_t axis;
        static MPG *instance;
        static void *xBinarySemaphore;
        RotaryEncoder *enc;
        bool enabled{false};
        volatile uint32_t last_count{0};
};
