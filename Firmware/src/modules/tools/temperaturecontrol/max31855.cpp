#include "max31855.h"
#include "Spi.h"
#include "Pin.h"
#include "ConfigReader.h"
#include "OutputStream.h"
#include "StringUtils.h"
#include "SlowTicker.h"

#include <math.h>

#define spi_cs_pin_key "spi_select_pin"
#define spi_channel_key "spi_channel"
#define readings_per_second_key "readings_per_second"

/*
    We have one SPI instance for all the max31855 chips
    each chip needs a unique chip select
    we create one timer or task to read the temp from each chip
    and return that value when asked (from ISR) so we avoid doing SPI over an ISR
*/

SPI *MAX31855::spi = nullptr;

// Get configuration from the config file
bool MAX31855::configure(ConfigReader& cr, ConfigReader::section_map_t& m, const char *def)
{
    int spi_channel = cr.get_int(m, spi_channel_key, -1);
    if(spi == nullptr) {
        // setup singleton spi instance
        if(spi_channel < 0 || spi_channel >= SPI::get_n_channels()) {
            printf("ERROR: MAX31855 illegal SPI channel %d\n", spi_channel);
            return false;
        }

        bool ok = false;
        spi = SPI::getInstance(spi_channel);
        if(spi != nullptr) {
            if(spi->init(16, 0, 1000000)) { // 16bit, mode0, 1MHz
                ok = true;
            }
        }
        if(!ok) {
            printf("ERROR: MAX31855 failed to get SPI channel %d\n", spi_channel);
            return false;
        }

    } else {
        // check we used the same spi channel
        if(spi->get_channel() != spi_channel) {
            printf("ERROR: MAX31855 must use the same SPI channel for all max31855 sensors\n");
            return false;
        }
    }

    // Chip select
    std::string cs_pin = cr.get_string(m, spi_cs_pin_key, "nc");
    cs = new Pin(cs_pin.c_str(), Pin::AS_OUTPUT);
    if(!cs->connected()) {
        delete cs;
        printf("ERROR: config_MAX31855: spi cs pin is invalid: %s\n", cs_pin.c_str());
        return false;
    }
    cs->set(true);
    printf("DEBUG: configure-MAX31855: spi cs pin: %s\n", cs->to_string().c_str());

    // start timer to read temperature from SPI
    int readings_per_second = cr.get_int(m, readings_per_second_key, 20);
    SlowTicker::getInstance()->attach(readings_per_second, std::bind(&MAX31855::do_read, this));

    return true;
}

// returns an average of the last few temperature values we've read
float MAX31855::get_temperature()
{
    return average_temperature;
}

void MAX31855::get_raw(OutputStream& os)
{
    spi->begin_transaction();
    cs->set(false);
    // Read two 16 bit words
    uint16_t data[2], dummy[2];

    if(!spi->write_read(dummy, data, 2)) {
        os.printf("error reading from spi\n");
        return;
    }
    cs->set(true);
    spi->end_transaction();

    os.printf("MAX31855 raw read %04X, %04X, errors: %d\n", data[0], data[1], errors);
    errors= 0;
    if(data[0] & 1) {
        os.printf("  error detected: %02X - ", data[1] & 0x07);
        if(data[1] & 8) { // this presumes SPI MISO is pulled high
            os.printf("no SPI slave present\n");
        }else{
            if(data[1] & 1) os.printf("open circuit ");
            if(data[1] & 2) os.printf("short to ground ");
            if(data[1] & 4) os.printf("short to Vcc ");
            os.printf("\n");
        }
    } else {
        float t = (((int16_t)data[0]) >> 2) / 4.0F;
        os.printf("  Thermocouple temp: %f\n", t);
        os.printf("  Internal temp: %f\n", (((int16_t)data[1]) >> 4) / 16.0F);
        os.printf("  min= %f, max= %f\n", min_temp, max_temp);
        min_temp = max_temp = t;
    }
}

// ask the temperature sensor hardware for a value
void MAX31855::do_read()
{
    spi->begin_transaction();
    cs->set(false);
    // Read 16 bits
    uint16_t data, dummy;
    bool ok = spi->write_read(&dummy, &data, 1);

    cs->set(true);
    spi->end_transaction();

    // Process temp
    float temperature= infinityf();
    if(ok && (data & 1)==0) {
        temperature= (((int16_t)data) >> 2) / 4.0F;
    }

    // do rolling average of last 4 readings, if all are inf then flag error
    readings[cnt&0x03]= temperature;
    ++cnt;

    if(cnt < 4) {
        // not enough samples yet
        return;
    }

    float sum = 0;
    int n= 0;
    for (int i = 0; i < 4; i++) {
        float t= readings[i];
        if(!isinf(t)) {
            sum += t;
            ++n;
            // keep track of min/max for M305
            if(t > max_temp) max_temp = t;
            if(t < min_temp) min_temp = t;
        }
    }

    if(n > 0) {
        average_temperature = sum / n;
    }else{
        // we got 4 readings of inf in a row so we have a problem
        average_temperature= infinityf();
        ++errors;
    }
 }
