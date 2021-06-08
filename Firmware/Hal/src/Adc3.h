#pragma once

#include <stdint.h>

class Adc3
{
public:
    static Adc3 *getInstance() { if(instance == nullptr){instance= new Adc3();} return instance; }

    // debug only
    static void deinit() { delete instance; }

    // parse an ADC3_1,10 string where ,10 is optional scale, return -1 if any error otherwise returns the channel
    int32_t from_string(const char *s, float& scale);

    float read_temp();
    float read_voltage(int32_t channel);
    uint32_t get_errors() const { return not_ready_error; }
    bool allocate(int32_t ch) const;
    bool is_valid() const { return valid; }

    static int get_max_value() { return 65535;} // 16bit samples

private:
    static Adc3 *instance;

    Adc3();
    virtual ~Adc3();

    bool valid{false};
    uint32_t not_ready_error{0};
};
