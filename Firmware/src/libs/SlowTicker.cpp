#include "SlowTicker.h"

#include <cstdio>

#include "FreeRTOS.h"
#include "timers.h"


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
    taskENTER_CRITICAL();
    std::function<void(void)> cb= instance->callbacks[idx];
    taskEXIT_CRITICAL();
    if(cb) {
        cb();
    }
}

SlowTicker::SlowTicker()
{
}

SlowTicker::~SlowTicker()
{
}

int SlowTicker::attach(uint32_t frequency, std::function<void(void)> cb)
{
    taskENTER_CRITICAL();
    uint32_t interval= 1000/frequency;
    uint32_t n= callbacks.size();
    char buf[32];
    snprintf(buf, sizeof(buf), "SlowTimer%lu", n);
    TimerHandle_t timer_handle= xTimerCreate(buf, pdMS_TO_TICKS(interval), pdTRUE, (void *)n, timer_handler);
    timers.push_back(timer_handle);
    callbacks.push_back(cb);
    taskEXIT_CRITICAL();

    if(xTimerStart(timer_handle, 1000) != pdPASS ) {
        // The timer could not be set into the Active state
        printf("ERROR: Failed to start the timer: %lu\n", n);
        return -1;
    }

    printf("DEBUG: SlowTicker added freq: %luHz, period: %lums\n", frequency, interval);

    // return the index it is in
    return n;
}

void SlowTicker::detach(int n)
{
    // TODO need to remove it but that would change all the indexes
    // For now we just zero the handle
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
