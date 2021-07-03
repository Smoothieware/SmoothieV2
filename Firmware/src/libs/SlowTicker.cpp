#include "SlowTicker.h"

#include <cstdio>

#include "FreeRTOS.h"
#include "timers.h"


// maximum frequency we can set
#define MAX_FREQUENCY configTICK_RATE_HZ

SlowTicker *SlowTicker::instance= nullptr;

// This module uses a Timer to periodically call registered callbacks
// Modules register with a function ( callback ) and a frequency, and we then call that function at the given frequency.

SlowTicker *SlowTicker::getInstance()
{
    if(instance == nullptr) {
        instance= new SlowTicker;
    }

    return instance;
}

void SlowTicker::deleteInstance()
{
    if(instance != nullptr && instance->started) {
        instance->stop();
    }
    delete instance;
    instance= nullptr;
}

static void timer_handler(TimerHandle_t xTimer)
{
    SlowTicker::getInstance()->tick();
}

SlowTicker::SlowTicker()
{
    max_frequency= 10; // 10 Hz
    interval = 1000/max_frequency; // interval in ms default to 10HZ, 100ms period
    timer_handle= xTimerCreate("SlowTickerTimer", pdMS_TO_TICKS(interval), pdTRUE, nullptr, timer_handler);
}

SlowTicker::~SlowTicker()
{
    if(timer_handle != nullptr) {
        xTimerDelete((TimerHandle_t)timer_handle, 1000);
    }
    timer_handle= nullptr;
}

bool SlowTicker::start()
{
    if(started) return true;

    // Start the timer
    if( xTimerStart( (TimerHandle_t)timer_handle, 1000 ) != pdPASS ) {
        // The timer could not be set into the Active state
        printf("ERROR: Failed to start the timer\n");
        return false;
    }
    started= true;
    return true;
}

bool SlowTicker::stop()
{
    if(!started) return true;

    if( xTimerStop((TimerHandle_t)timer_handle, 1000) != pdPASS) {
        printf("ERROR: Failed to stop the timer\n");
        return false;
    }
    started= false;
    return true;
}

int SlowTicker::attach(uint32_t frequency, std::function<void(void)> cb)
{
    uint32_t period = 1000 / frequency; // period in ms
    int countdown = period;

    if( frequency > max_frequency ) {
        // reset frequency to a higher value
        if(!set_frequency(frequency)) {
            printf("WARNING: SlowTicker cannot be set to > %luHz\n", MAX_FREQUENCY);
            return -1;
        }
        max_frequency = frequency;
    }

    printf("DEBUG: SlowTicker added freq: %luHz, period: %lums\n", frequency, period);
    bool was_started= started;
    if(was_started) stop();
    callbacks.push_back(std::make_tuple(countdown, period, cb));
    if(was_started) start();

    // return the index it is in
    return callbacks.size()-1;
}

void SlowTicker::detach(int n)
{
    // TODO need to remove it but that would change all the indexes
    // For now we just zero the callback
    std::get<2>(callbacks[n])= nullptr;
}

// Set the base frequency we use for all sub-frequencies
// NOTE this is a slow ticker so ticks faster than 1000Hz are not allowed
bool SlowTicker::set_frequency( int frequency )
{
    if(frequency > (int)MAX_FREQUENCY) return false;
    this->interval = 1000 / frequency; // millisecond interval
    if(started) {
        stop(); // must stop timer first
        // change frequency of timer callback
        if( xTimerChangePeriod( (TimerHandle_t)timer_handle, pdMS_TO_TICKS(interval), 1000 ) != pdPASS ) {
            printf("ERROR: Failed to change timer period\n");
            return false;
        }
        start(); // restart it
    }
    return true;
}

// This is a timer calback and must not block
void SlowTicker::tick()
{
    // Call all callbacks that need to be called
    for(auto& i : callbacks) {
        int& countdown= std::get<0>(i);
        countdown -= this->interval;
        if (countdown <= 0) {
            countdown += std::get<1>(i);
            auto& fnc= std::get<2>(i); // get the callback
            if(fnc) {
                fnc();
            }
        }
    }
}
