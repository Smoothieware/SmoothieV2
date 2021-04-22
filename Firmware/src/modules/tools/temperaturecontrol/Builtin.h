#pragma once

#include "TempSensor.h"

class OutputStream;

class Builtin : public TempSensor
{
    public:
        Builtin();
        virtual ~Builtin();

        // TempSensor interface.
        bool configure(ConfigReader& cr, ConfigReader::section_map_t&);
        float get_temperature();
    private:
    	bool valid{false};
};
