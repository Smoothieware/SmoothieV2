#pragma once

#include <vector>
#include <tuple>
#include <functional>
#include <cstdint>

class SlowTicker
{
    public:
        static SlowTicker *getInstance();
        static void deleteInstance();

        // delete copy and move constructors and assign operators
        SlowTicker(SlowTicker const&) = delete;             // Copy construct
        SlowTicker(SlowTicker&&) = delete;                  // Move construct
        SlowTicker& operator=(SlowTicker const&) = delete;  // Copy assign
        SlowTicker& operator=(SlowTicker &&) = delete;      // Move assign

        // call back frequency in Hz
        int attach(uint32_t frequency, std::function<void(void)> cb);
        void detach(int n);
        bool start();
        bool stop();

    private:
        static SlowTicker *instance;
        SlowTicker();
        virtual ~SlowTicker();
        static void timer_handler(void *xTimer);
        std::vector<void *> timers;
        std::vector<std::function<void(void)>> callbacks;
        bool started{false};
};
