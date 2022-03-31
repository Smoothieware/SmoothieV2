#pragma once

#include <stdint.h>
#include <string>
#include <set>

class Pin;

class Adc
{
public:
    static Adc *getInstance(int n) { return (n < num_channels) ? instances[n] : nullptr; }

    Adc(const char *);
    virtual ~Adc();

    static bool deinit();
    static bool post_config_setup();
    static bool start();
    static bool stop();
    static void on_tick(void);

    // specific to each instance
    uint32_t read();
    uint16_t read_raw();
    float read_voltage();
    int get_channel() const { return channel; }
    uint32_t get_errors() const { return not_ready_error; }
    bool is_valid() const { return valid; }
    std::string to_string() const;

    static int get_max_value() { return 65535;} // 16bit samples

    static void sample_isr(bool);
    static std::set<uint16_t> allocated_channels;
    static const int num_channels= 7;
    static const int num_samples= 8;

private:
    static Adc* instances[num_channels];
    static bool running;

    std::string name;
    bool valid{false};
    uint16_t channel;
    uint32_t not_ready_error{0};
    // buffer storing the last num_samples readings for each channel instance
    uint16_t sample_buffer[num_samples]{0};
    uint32_t last_sample{0};
};

