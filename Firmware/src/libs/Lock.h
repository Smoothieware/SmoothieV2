#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

#include <mutex>

// locks a global mutex and releases when out of scope
class Lock
{
public:
    Lock(SemaphoreHandle_t l) : xSemaphore(l) {}
    void lock()
    {
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
    }
    void unlock()
    {
        xSemaphoreGive(xSemaphore);
    };

private:
    SemaphoreHandle_t xSemaphore;
};

class AutoLock
{
public:
    AutoLock(void *mutex) : l(mutex) { xSemaphoreTake(l, portMAX_DELAY); }
    ~AutoLock(){ xSemaphoreGive(l); }
private:
    void *l;
};
