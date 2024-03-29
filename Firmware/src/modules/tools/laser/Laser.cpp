#include "Laser.h"

#include "Block.h"
#include "Robot.h"
#include "Dispatcher.h"
#include "SlowTicker.h"
#include "FastTicker.h"
#include "Pwm.h"
#include "Pin.h"
#include "StepTicker.h"
#include "ConfigReader.h"
#include "GCode.h"
#include "OutputStream.h"
#include "StringUtils.h"

#include <algorithm>

#define enable_key "enable"
#define pwm_pin_key "pwm_pin"
#define inverted_key "inverted_pwm"
#define pullup_key "pullup"
#define opendrain_key "opendrain"
#define ttl_pin_key "ttl_pin"
#define maximum_power_key "maximum_power"
#define minimum_power_key "minimum_power"
#define maximum_s_value_key "maximum_s_value"
#define default_power_key "default_power"
#define proportional_power_key "proportional_power"

REGISTER_MODULE(Laser, Laser::create)

bool Laser::create(ConfigReader& cr)
{
    printf("DEBUG: configure laser\n");
    Laser *laser = new Laser();
    if(!laser->configure(cr)) {
        printf("INFO: No laser configured\n");
        delete laser;
        laser = nullptr;
    }
    return true;
}

Laser::Laser() : Module("laser")
{
    laser_on = false;
    scale = 1;
    manual_fire = false;
    disable_auto_power = false;
    fire_duration = 0;
}

bool Laser::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("laser", m)) return false;

    if( !cr.get_bool(m,  enable_key , false) ) {
        // as not needed free up resource
        return false;
    }

    pwm_pin = new Pwm(cr.get_string(m, pwm_pin_key, "nc"));

    if(!pwm_pin->is_valid()) {
        printf("ERROR: laser-config: Specified pin is not a valid PWM pin.\n");
        delete pwm_pin;
        pwm_pin = nullptr;
        return false;
    }

    this->pwm_inverting = cr.get_bool(m, inverted_key, false);

    this->pwm_pin->set_pullup(cr.get_bool(m, pullup_key, true));
    this->pwm_pin->set_od(cr.get_bool(m, opendrain_key, false));

    // TTL settings
    ttl_pin = new Pin(cr.get_string(m, ttl_pin_key, "nc"), Pin::AS_OUTPUT_OFF);
    if(ttl_pin->connected()) {
        ttl_inverting = ttl_pin->is_inverting();
        ttl_pin->set(false);
    } else {
        ttl_used = false;
        delete ttl_pin;
        ttl_pin = NULL;
    }

    // this is set globally in Pwm
    //uint32_t period= cr.get_float(m, pwm_period_key, 20);
    //this->pwm_pin->period_us(period);

    this->pwm_pin->set(this->pwm_inverting ? 1 : 0);
    this->laser_maximum_power = cr.get_float(m, maximum_power_key, 1.0f);
    this->laser_minimum_power = cr.get_float(m, minimum_power_key, 0);

    // S value that represents maximum (default 1)
    this->laser_maximum_s_value = cr.get_float(m, maximum_s_value_key, 1.0f);
    // default s value for laser
    Robot::getInstance()->set_s_value(cr.get_float(m, default_power_key, 0.8F));
    disable_auto_power= !cr.get_bool(m, proportional_power_key, true);

    set_laser_power(0);

    // register command handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER->add_handler( "fire", std::bind( &Laser::handle_fire_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 221, std::bind(&Laser::handle_M221, this, _1, _2));

    // no point in updating the power more than the PWM frequency, but no more than 1KHz
    uint32_t pwm_freq = pwm_pin->get_frequency();
    uint32_t f = std::min(1000UL, pwm_freq);
    if(f >= FastTicker::get_min_frequency()) {
        if(FastTicker::getInstance()->attach(f, std::bind(&Laser::set_proportional_power, this)) < 0) {
            printf("ERROR: configure-laser: Fast Ticker was not set (Too slow?)\n");
            return false;
        }

    } else {
        if(SlowTicker::getInstance()->attach(f, std::bind(&Laser::set_proportional_power, this)) < 0) {
            printf("ERROR: configure-laser: Slow Ticker was not set (Too fast?)\n");
            return false;
        }
    }

    ms_per_tick = 1000 / f;

    return true;
}

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }

bool Laser::handle_fire_cmd( std::string& params, OutputStream& os )
{
    HELP("fire laser: 0-100 | off");

    if(Module::is_halted()) {
        os.printf("ignored while in ALARM state\n");
        return true; // if in halted state ignore any commands
    }

    std::string power = stringutils::shift_parameter(params);
    if(power.empty()) {
        os.printf("Usage: fire power%% [durationms]|off|status\n");
        return true;
    }

    float p;
    fire_duration = 0; // By default unlimited
    if(power == "status") {
        os.printf("laser manual state: %s\n", manual_fire ? "on" : "off");
        return true;
    }

    if(power == "off" || power == "0") {
        p = 0;
        os.printf("turning laser off and returning to auto mode\n");
    } else {
        p = strtof(power.c_str(), NULL);
        if(p <= 0) {
            os.printf("Usage: fire power%% [durationms]|off|status\n");
            return true;
        }

        if(p > 100) p = 100;

        std::string duration = stringutils::shift_parameter(params);
        if(!duration.empty()) {
            fire_duration = atoi(duration.c_str());
            // Avoid negative values, its just incorrect
            if(fire_duration < ms_per_tick) {
                os.printf("WARNING: Minimal duration is %ld ms, not firing\n", ms_per_tick);
                return true;
            }
            // rounding to minimal value
            if (fire_duration % ms_per_tick != 0) {
                fire_duration = (fire_duration / ms_per_tick) * ms_per_tick;
            }
            os.printf("WARNING: Firing laser at %1.2f%% power, for %ld ms, use fire off to stop test fire earlier\n", p, fire_duration);
        } else {
            os.printf("WARNING: Firing laser at %1.2f%% power, entering manual mode use fire off to return to auto mode\n", p);
        }
    }

    p = p / 100.0F;
    manual_fire = set_laser_power(p);

    return true;
}

// returns instance
bool Laser::request(const char *key, void *value)
{
    if(strcmp(key, "get_current_power") == 0) {
        *((float*)value) = get_current_power();
        return true;
    }

    return false;
}

bool Laser::handle_M221(GCode& gcode, OutputStream& os)
{
    if(gcode.has_no_args()) {
        os.printf("Laser power: %6.2f %%, disable auto power: %d, PWM frequency: %lu Hz\n", this->scale * 100.0F, disable_auto_power, pwm_pin->get_frequency());

    } else {
        if(gcode.has_arg('S')) {
            this->scale = gcode.get_arg('S') / 100.0F;
        }

        if(gcode.has_arg('P')) {
            disable_auto_power = gcode.get_int_arg('P') > 0;
        }

        if(gcode.has_arg('R')) {
            uint32_t freq = gcode.get_int_arg('R');
            pwm_pin->set_frequency(freq);
        }

    }

    return true;
}

// calculates the current speed ratio from the currently executing block
float Laser::current_speed_ratio(const Block *block) const
{
    // find the primary moving actuator (the one with the most steps)
    size_t pm = 0;
    uint32_t max_steps = 0;
    for (size_t i = 0; i < Robot::getInstance()->get_number_registered_motors(); i++) {
        // find the motor with the most steps
        if(block->steps[i] > max_steps) {
            max_steps = block->steps[i];
            pm = i;
        }
    }

    // figure out the ratio of its speed, from 0 to 1 based on where it is on the trapezoid,
    // this is based on the fraction it is of the requested rate (nominal rate)
    float ratio = block->get_trapezoid_rate(pm) / block->nominal_rate;

    return ratio;
}

// get laser power for the currently executing block, returns false if nothing running or a G0
bool Laser::get_laser_power(float& power) const
{
    const Block *block = StepTicker::getInstance()->get_current_block();

    // Note to avoid a race condition where the block is being cleared we check the is_ready flag which gets cleared first,
    // as this is an interrupt if that flag is not clear then it cannot be cleared while this is running and the block will still be valid (albeit it may have finished)
    if(block != nullptr && block->is_ready && block->is_g123) {
        float requested_power = ((float)block->s_value / (1 << 11)) / this->laser_maximum_s_value; // s_value is 1.11 Fixed point
        float ratio = 1.0F;
        if(!disable_auto_power) { // true to disable auto power
            ratio = current_speed_ratio(block);
        }
        power = requested_power * ratio * scale;

        return true;
    }

    return false;
}

// called every millisecond from timer ISR
void Laser::set_proportional_power(void)
{
    if(manual_fire) {
        // If we have fire duration set
        if (fire_duration > 0) {
            // Decrease it each ms
            fire_duration -= ms_per_tick;
            // And if it turned 0, disable laser and manual fire mode
            if (fire_duration <= 0) {
                set_laser_power(0);
                manual_fire = false;
                fire_duration = 0;
            }
        }
        return;
    }

    float power;
    if(get_laser_power(power)) {
        // adjust power to maximum power and actual velocity
        float proportional_power = ( (this->laser_maximum_power - this->laser_minimum_power) * power ) + this->laser_minimum_power;
        set_laser_power(proportional_power);

    } else if(laser_on) {
        // turn laser off
        set_laser_power(0);
    }
    return;
}

bool Laser::set_laser_power(float power)
{
    // Ensure power is >=0 and <= 1
    if(power < 0) power = 0;
    else if(power > 1) power = 1;

    if(power > 0.00001F) {
        this->pwm_pin->set(this->pwm_inverting ? 1 - power : power);
        if(!laser_on && this->ttl_used) this->ttl_pin->set(true);
        laser_on = true;

    } else {
        this->pwm_pin->set(this->pwm_inverting ? 1 : 0);
        if (this->ttl_used) this->ttl_pin->set(false);
        laser_on = false;
    }

    return laser_on;
}

void Laser::on_halt(bool flg)
{
    if(flg) {
        set_laser_power(0);
        manual_fire = false;
    }
}

float Laser::get_current_power() const
{
    if(pwm_pin == nullptr) return 0;
    float p = pwm_pin->get();
    return (this->pwm_inverting ? 1 - p : p) * 100;
}
