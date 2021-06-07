#include "Builtin.h"
#include "Adc3.h"

Builtin::Builtin()
{
}

Builtin::~Builtin()
{
}

// Set it up
bool Builtin::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    return true;
}

float Builtin::get_temperature()
{
    Adc3 *adc= Adc3::getInstance();
    float t= adc->read_temp();
    return t;
}
