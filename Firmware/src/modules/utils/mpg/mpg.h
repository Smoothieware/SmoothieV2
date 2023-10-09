#pragma once

#include "Module.h"
#include "Pin.h"
#include "ConfigReader.h"

#include <string>

class GCode;
class OutputStream;
class RotaryEncoder;

class MPG : public Module {
    public:
        MPG();
        static bool create(ConfigReader& cr);

    private:
        bool configure(ConfigReader& cr, ConfigReader::section_map_t& m, const char *name);
        bool handle_cmd(std::string& params, OutputStream& os);
        void handle_change();
        static void vHandlerTask(void *pvParameters);
        void check_encoder();

        uint8_t axis;
        void *xBinarySemaphore;
        RotaryEncoder *enc;
        volatile uint32_t last_count{0};
};
