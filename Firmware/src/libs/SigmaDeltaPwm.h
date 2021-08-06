#pragma once

#include "Pin.h"

#include <set>

class SigmaDeltaPwm : public Pin {
public:
    SigmaDeltaPwm(const char *);
    virtual ~SigmaDeltaPwm();

    void     max_pwm(int);
    int      max_pwm(void);

    void     pwm(int);
    int      get_pwm() const { return _pwm; }
    void     set(bool);

private:
    static std::set<SigmaDeltaPwm*> instances;
    static void global_tick(void);
    void on_tick(void);

    static int fastticker;
    int  _max;
    int  _pwm;
    int  _sd_accumulator;
    bool _sd_direction;
};
