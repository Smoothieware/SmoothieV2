#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <bitset>

class Pin
{
public:
    Pin();
    Pin(const char *s);
    virtual ~Pin();

    enum TYPE_T {AS_INPUT, AS_OUTPUT};
    Pin(const char *s, Pin::TYPE_T);


    Pin* from_string(std::string value);
    std::string to_string() const;

    bool connected() const
    {
        return this->valid;
    }

    Pin* as_output();
    Pin* as_input();

    // we need to do this inline
    inline bool get() const
    {
        if (!this->valid) return false;
        // IDR
        return ((this->pport[4] & this->ppin) != 0x00U) ^ this->inverting;
    }

    // we need to do this inline
    inline void set(bool value)
    {
        if (!this->valid) return;
        // BSSR
        if(this->inverting ^ value) {
            this->pport[6] = this->ppin;
        }else{
            this->pport[6] = this->ppin << 16;
        }
    }

    inline uint16_t get_gpioport() const { return this->gpioport; }
    inline uint16_t get_gpiopin() const { return this->gpiopin; }

    bool is_inverting() const { return inverting; }
    void set_inverting(bool f) { inverting = f; }

    // mbed::InterruptIn *interrupt_pin();


private:

    static bool set_allocated(uint8_t, uint8_t, bool set= true);
    uint32_t *pport;
    uint32_t ppin;
    struct {
        uint8_t gpiopin:6;
        char gpioport:8; // the port A-K
        bool inverting: 1;
        bool open_drain: 1;
        bool pullup:1;
        bool pulldown:1;
        bool valid: 1;
        bool adc_only: 1;   //true if adc only pin
        int adc_channel: 8;   //adc channel
    };
};

