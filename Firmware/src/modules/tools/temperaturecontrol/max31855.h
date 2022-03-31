#pragma once

#include "TempSensor.h"

class OutputStream;
class SPI;
class Pin;

class MAX31855 : public TempSensor
{
    public:
        MAX31855() {};
        virtual ~MAX31855() {};

        // TempSensor interface.
        bool configure(ConfigReader& cr, ConfigReader::section_map_t&, const char *defadc);
        float get_temperature();
        void get_raw(OutputStream& os);

    private:
        void do_read();
        static SPI *spi;
        Pin *cs;
        float min_temp{999}, max_temp{-999};
        float average_temperature{0};
        float readings[4]{0};
        uint32_t cnt{0};
        uint32_t errors{0};
};
