#pragma once

#include <stdint.h>

class Adc3
{
public:
    static Adc3 *getInstance() { if(instance == nullptr){instance= new Adc3();} return instance; }

    // debug only
    void deinit() { delete instance; }

    float read_temp();
    float read_voltage(int32_t channel);
    uint32_t get_errors() const { return not_ready_error; }
    bool is_valid(int32_t ch) const;

    static int get_max_value() { return 65535;} // 16bit samples

private:
    static Adc3 *instance;

    Adc3();
    virtual ~Adc3();

    bool valid{false};
    uint32_t not_ready_error{0};
};
