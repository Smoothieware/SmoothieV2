#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <bitset>
#include <functional>

class Pin
{
public:
    Pin();
    Pin(const char *s);
    virtual ~Pin();

    enum TYPE_T {AS_INPUT, AS_OUTPUT, AS_OUTPUT_ON, AS_OUTPUT_OFF};
    Pin(const char *s, Pin::TYPE_T);

    bool from_string(const std::string& value);
    std::string to_string() const;

    bool deinit();

    bool connected() const { return this->valid; }

    bool as_output();
    bool as_input();
    enum INT_TYPE_T {RISING, FALLING, CHANGE};
    bool as_interrupt(std::function<void(void)> fnc, Pin::INT_TYPE_T rising=RISING, uint32_t pri=0x0F);

    // we need to do this inline
    inline bool get() const
    {
        if (!this->valid) return false;
        if(is_input) {
            // IDR
            return ((this->pport[4] & this->ppin) != 0x00U) ^ this->inverting;
        }else{
            // ODR
            return ((this->pport[5] & this->ppin) != 0x00U) ^ this->inverting;
        }
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
    void toggle();

    inline uint16_t get_gpioport() const { return this->gpioport; }
    inline uint16_t get_gpiopin() const { return this->gpiopin; }

    bool is_inverting() const { return inverting; }
    void set_inverting(bool f) { inverting = f; }

    bool is_interrupt() const { return interrupt; }

    static bool set_allocated(uint8_t, uint8_t, bool set= true);
    static bool allocate_interrupt_pin(uint8_t pin, bool set= true);
    static bool is_allocated(uint8_t port, uint8_t pin);
    static bool parse_pin(const std::string& value, char& port, uint16_t& pin, size_t& pos);

private:
    uint32_t *pport;
    uint32_t ppin;
    struct {
        uint8_t gpiopin:4; // the pin 0-15
        char gpioport:8; // the port A-K
        bool inverting: 1;
        bool open_drain: 1;
        bool pullup:1;
        bool pulldown:1;
        bool valid: 1;
        bool interrupt: 1;
        bool is_input:1;
    };
};

