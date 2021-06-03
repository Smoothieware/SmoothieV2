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

        bool start();
        bool stop();

        // call back frequency in Hz
        int attach(uint32_t frequency, std::function<void(void)> cb);
        void detach(int n);
        void tick();

    private:
        static SlowTicker *instance;
        SlowTicker();
        virtual ~SlowTicker();

        // set frequency of timer in Hz
        bool set_frequency( int frequency );

        using callback_t = std::tuple<int, uint32_t, std::function<void(void)>>;
        std::vector<callback_t> callbacks;
        uint32_t max_frequency{0};
        uint32_t interval{0}; // period in ms between calls
        void *timer_handle{nullptr};
        bool started{false};
};
