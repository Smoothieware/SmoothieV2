/**
 * code to talk to a Max7219 driving an 8 digit 7 segment display via Soft SPI
 * Ported from the example for pico...
 */

#include "max7219.h"
#include "ConfigReader.h"
#include "Pin.h"
#include "benchmark_timer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

const uint8_t CMD_NOOP = 0;
const uint8_t CMD_DIGIT0 = 1; // Goes up to 8, for each line, 0 is on the right
const uint8_t CMD_DECODEMODE = 9;
const uint8_t CMD_BRIGHTNESS = 10;
const uint8_t CMD_SCANLIMIT = 11;
const uint8_t CMD_SHUTDOWN = 12;
const uint8_t CMD_DISPLAYTEST = 15;


#define enable_key "enable"
#define clk_pin_key "clk"
#define mosi_pin_key "mosi"
#define cs_pin_key "cs"
#define cascaded_key "cascaded"

REGISTER_MODULE(MAX7219, MAX7219::create)

bool MAX7219::create(ConfigReader& cr)
{
    printf("DEBUG: configure MAX7219 display\n");
    MAX7219 *st = new MAX7219();
    if(!st->configure(cr)) {
        printf("INFO: MAX7219 not enabled\n");
        delete st;
    }
    return true;
}

MAX7219::MAX7219() : Module("max7219")
{
    plock = (void*)xSemaphoreCreateMutex();
}

MAX7219::~MAX7219()
{
    if(clk != nullptr) {
        delete clk;
    }
    if(mosi != nullptr) {
        delete mosi;
    }

    for(auto cs : cs_list) {
        if(cs != nullptr) {
            delete cs;
        }
    }
    if(plock != nullptr) {
        vSemaphoreDelete(plock);
    }
}

bool MAX7219::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("max7219", m)) return false;

    if(!cr.get_bool(m, enable_key, false)) {
        return false;
    }

    // if displays are cascaded or need individual cs pins
    cascaded = cr.get_int(m, cascaded_key, 0);
    if(cascaded != 0 && cascaded < 2) {
        printf("ERROR: config_max7219: cascaded must be 0 or >1\n");
        return false;
    }

    std::string clk_pin = cr.get_string(m, clk_pin_key, "nc");
    clk = new Pin(clk_pin.c_str(), Pin::AS_OUTPUT_OFF); // set low on creation
    if(!clk->connected()) {
        printf("ERROR: config_max7219: spi clk pin %s is invalid\n", clk_pin.c_str());
        return false;
    }
    printf("DEBUG: config_max7219: spi clk pin: %s\n", clk->to_string().c_str());

    std::string mosi_pin = cr.get_string(m, mosi_pin_key, "nc");
    mosi = new Pin(mosi_pin.c_str(), Pin::AS_OUTPUT_OFF); // set low on creation
    if(!mosi->connected()) {
        printf("ERROR: config_max7219: spi mosi pin %s is invalid\n", mosi_pin.c_str());
        return false;
    }
    printf("DEBUG: config_max7219: spi mosi pin: %s\n", mosi->to_string().c_str());

    if(cascaded != 0) {
        // just one cs pin for all displays
        std::string cs_pin = cr.get_string(m, cs_pin_key, "nc");
        Pin *cs = new Pin(cs_pin.c_str(), Pin::AS_OUTPUT_ON); // set high on creation
        if(!cs->connected()) {
            printf("ERROR: config_max7219: cascaded mode requires a cs pin to be defined\n");
            return false;
        }
        cs_list.push_back(cs);
        printf("DEBUG: config_max7219: cascaded cs pin: %s\n", cs->to_string().c_str());
    }
    return true;
}

int MAX7219::add_instance(const char *cs_pin)
{
    if(cascaded > 0) {
        printf("ERROR: max7219.add_instance(): cannot add_instance for cascaded displays\n");
        return -1;
    }

    Pin *cs = new Pin(cs_pin, Pin::AS_OUTPUT_ON); // set high on creation
    if(!cs->connected()) {
        printf("ERROR: max7219.add_instance(): spi cs pin %s is invalid\n", cs_pin);
        return -1;
    }
    int id = cs_list.size();
    cs_list.push_back(cs);
    printf("DEBUG: max7219.add_instance(): id %d, cs pin: %s\n", id, cs->to_string().c_str());
    return id;
}

bool MAX7219::lock()
{
    if(plock != nullptr) {
        // take lock
        uint32_t t = 0; // pdMS_TO_TICKS(10);
        if(xSemaphoreTake(plock, t) != pdTRUE) {
            return false;
        }
    }

    return true;
}

void MAX7219::unlock()
{
    if(plock != nullptr) {
        // release lock
        xSemaphoreGive(plock);
    }
}

// use benchmark timer as it has the resolution needed
static inline void wait_ns(uint32_t ns)
{
    uint32_t st = benchmark_timer_start();
    while(benchmark_timer_as_ns(benchmark_timer_elapsed(st)) < ns) ;
}

void MAX7219::spi_write(uint16_t v)
{
    for (int b = 15; b >= 0; --b) {
        mosi->set(((v >> b) & 0x01) != 0);
        wait_ns(25);
        clk->set(true);
        wait_ns(50);
        clk->set(false);
        wait_ns(50);
    }
}

void MAX7219::cs_select(int id, bool flg)
{
    if(cascaded > 0) id = 0;
    cs_list[id]->set(!flg);
}

// write to specified display, the other displays get NOP
void MAX7219::write_register_to(int disp, uint8_t reg, uint8_t data)
{
    cs_select(0, true);
    wait_ns(25);
    for (int i = cascaded - 1; i >= 0; --i) {
        if(disp == i) {
            spi_write((reg << 8) | data);
        } else {
            spi_write(CMD_NOOP);
        }
    }
    wait_ns(50);
    cs_select(0, false);
    wait_ns(50);
}

void MAX7219::write_register(int id, uint8_t reg, uint8_t data)
{
    if(cascaded > 0) {
        if(id >= cascaded) return;
        write_register_to(id, reg, data);
    } else {
        if(id < 0 || id >= (int)cs_list.size()) return;
        cs_select(id, true);
        spi_write((reg << 8) | data);
        cs_select(id, false);
        wait_ns(50);
    }
}

// displays integer right aligned
// if negative displays '-' in first digit
// if leading_zeroes is true then 0 is displayed in left digits
// otherwise blanks
void MAX7219::display_int(int id, int32_t num, bool leading_zeros)
{
    int digit = 0;
    if(num == 0) {
        if(leading_zeros) {
            for (int i = 0; i < 8; i++) {
                write_register(id, CMD_DIGIT0 + i, 0);
            }
        } else {
            write_register(id, CMD_DIGIT0, 0);
        }
        return;
    }

    int ndigits = 8;
    if(num < 0) {
        num = -num;
        write_register(id, CMD_DIGIT0 + 7, 0b1010); // display -
        ndigits = 7;
    }
    while (num && digit < ndigits) {
        // start at right most digit and work to the left
        write_register(id, CMD_DIGIT0 + digit, num % 10);
        num /= 10;
        digit++;
    }

    if(digit < 8) {
        for (int i = digit; i < ndigits; i++) {
            write_register(id, CMD_DIGIT0 + i, leading_zeros ? 0 : 0x0F);
        }
    }
}

// display a float to 3dp
void MAX7219::display_float3(int id, float num)
{
    int ndigits = 8;
    if(num < 0) {
        num = -num;
        write_register(id, CMD_DIGIT0 + 7, 0b1010); // display -
        ndigits = 7;
    }
    float f = num + 0.00055555F; // round up 3dp
    int32_t fi = f * 1000; // take 3 dp and truncate

    // left most 3 digits are fraction
    for (int i = 0; i < 3; ++i) {
        write_register(id, CMD_DIGIT0 + i, fi % 10);
        fi /= 10;
    }

    // next digit has decimal point
    write_register(id, CMD_DIGIT0 + 3, (fi % 10) | 0x80);
    fi /= 10;

    for (int i = 4; i < ndigits; ++i) {
        if(fi == 0) {
            // blank rest of digits
            write_register(id, CMD_DIGIT0 + i, 0x0F);
        } else {
            write_register(id, CMD_DIGIT0 + i, fi % 10);
            fi /= 10;
        }
    }
}

// blanks display
void MAX7219::clear(int id)
{
    for (int i = 0; i < 8; i++) {
        write_register(id, CMD_DIGIT0 + i, 0x0F);
    }
}

void MAX7219::init()
{
    int n= (cascaded > 0) ? cascaded : (int)cs_list.size();

    for (int i = 0; i < n; ++i) {
        // Send init sequence to all devices
        write_register(i, CMD_SHUTDOWN, 0);
        write_register(i, CMD_DISPLAYTEST, 0);
        write_register(i, CMD_SCANLIMIT, 7);
        write_register(i, CMD_DECODEMODE, 255);
        write_register(i, CMD_SHUTDOWN, 1);
        write_register(i, CMD_BRIGHTNESS, 8);

        clear(i);
    }
}

/*
    example config.ini entry:-

# for cascaded modules...
[max7219]
enable = false
clk = PJ7
mosi = PE8
cascaded = 3
cs = PJ8

# for modules with a chip select each... Caller specifies cs pin
[max7219]
enable = false
clk = PJ7
mosi = PE8


*/
