#include "SlowTicker.h"

#include <cstdio>

#include "FreeRTOS.h"
#include "timers.h"
#include "semphr.h"

// maximum frequency we can set
#define MAX_FREQUENCY configTICK_RATE_HZ

SlowTicker *SlowTicker::instance= nullptr;

// This module uses FreeRTOS Timers to periodically call registered callbacks
// Modules register with a function ( callback ) and a frequency

SlowTicker *SlowTicker::getInstance()
{
    if(instance == nullptr) {
        instance= new SlowTicker;
    }

    return instance;
}

void SlowTicker::deleteInstance()
{
    if(instance != nullptr) {
        for (size_t i = 0; i < instance->timers.size(); ++i) {
            instance->detach(i);
        }
    }
    delete instance;
    instance= nullptr;
}

void SlowTicker::timer_handler(TimerHandle_t xTimer)
{
    int idx= (int)pvTimerGetTimerID(xTimer);
    auto& cb= instance->callbacks[idx];
    if(cb) {
        cb();
    }
}

SlowTicker::SlowTicker()
{}

SlowTicker::~SlowTicker()
{}

bool SlowTicker::start()
{
    // start all timers
    for(auto i : timers) {
        if(xTimerStart(i, 1000) != pdPASS ) {
            // The timer could not be set into the Active state
            printf("ERROR: Failed to start the timer: %s\n", pcTimerGetName(i));
            return false;
        }
    }
    started= true;
    return true;
}

bool SlowTicker::stop()
{
    int cnt= 0;
    for(auto i : timers) {
        ++cnt;
        if(xTimerStop(i, 1000) != pdPASS ) {
            // The timer could not be set into the Active state
            printf("ERROR: Failed to stop the timer: %s\n", pcTimerGetName(i));
            return false;
        }
    }
    started= false;
    return true;
}

// WARNING attach can only be called when timers have not yet been started
// TODO if we need to do this then we need to make sure a timer cannot fire while we update callbacks and timers
int SlowTicker::attach(uint32_t frequency, std::function<void(void)> cb)
{
    if(started) {
        printf("ERROR: Cannot attach a slowtimer when timers are running\n");
        return -1;
    }

    if(frequency > 1000) {
        printf("WARNING: Cannot set a slowtimer to frequencies > 1KHz, set to 1KHz\n");
        frequency= 1000;

    } else if(frequency == 0) {
        printf("WARNING: Cannot set a slowtimer to 0Hz frequency, set to 1Hz\n");
        frequency= 1;
    }

    uint32_t interval= 1000/frequency;
    uint32_t n= callbacks.size();
    char buf[32];
    snprintf(buf, sizeof(buf), "SlowTicker%lu", n);
    TimerHandle_t timer_handle= xTimerCreate(buf, pdMS_TO_TICKS(interval), pdTRUE, (void *)n, timer_handler);
    if(timer_handle == NULL) {
        printf("ERROR: SlowTicker failed to create timer\n");
        return -1;
    }
    timers.push_back(timer_handle);
    callbacks.push_back(cb);

    printf("DEBUG: SlowTicker %lu added freq: %luHz, period: %lums\n", n, frequency, interval);

    // return the index it is in
    return n;
}

void SlowTicker::detach(int n)
{
    // TODO need to remove it but that would change all the indexes
    // For now we just zero the handle
    if(started) {
        printf("WARNING: Cannot deattach a slowtimer when timers are running\n");

    }

    TimerHandle_t timer_handle= timers[n];
    if(timer_handle != nullptr) {
        if(xTimerStop(timer_handle, 1000) != pdPASS) {
            printf("ERROR: Failed to stop the timer %d\n", n);
        }
        xTimerDelete(timer_handle, 1000);
        timers[n]= nullptr;
        callbacks[n]= nullptr;
    }
}
