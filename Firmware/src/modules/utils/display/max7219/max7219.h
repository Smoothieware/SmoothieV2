#pragma once

#include "Module.h"
#include "Pin.h"

#include <vector>

class ConfigReader;

class MAX7219 : public Module
{

public:
    // Constructor
    MAX7219();
    virtual ~MAX7219();

    static bool create(ConfigReader& cr);
    bool configure(ConfigReader& cr);
    int add_instance(const char *cs_pin);

    void display_int(int id, int32_t num, bool leading_zeros=false);
    void display_float3(int id, float num);

    void clear(int id);
    void init();
    bool is_cascaded() const { return cascaded != 0; }

    // Mutex to stop concurrent access, the caller is responsible for locking and unlocking access
    bool lock();
    void unlock();

private:
    void spi_write(uint16_t b);
    void write_register_to(int disp, uint8_t reg, uint8_t data);
    void write_register(int id, uint8_t reg, uint8_t data);

    void cs_select(int id, bool flg);
    int cascaded{0};
    Pin *clk{nullptr};
    Pin *mosi{nullptr};
    std::vector<Pin *> cs_list;
    void *plock{nullptr};
};
