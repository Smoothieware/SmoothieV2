#pragma once

#include <vector>
#include <tuple>
#include <functional>
#include <cstdint>

class FastTicker
{
    public:
        static FastTicker *getInstance();
        static void deleteInstance();

        // delete copy and move constructors and assign operators
        FastTicker(FastTicker const&) = delete;             // Copy construct
        FastTicker(FastTicker&&) = delete;                  // Move construct
        FastTicker& operator=(FastTicker const&) = delete;  // Copy assign
        FastTicker& operator=(FastTicker &&) = delete;      // Move assign

        bool start();
        bool stop();

        // call back frequency in Hz
        int attach(uint32_t frequency, std::function<void(void)> cb);
        void detach(int n);
        void tick();
        bool is_running() const { return started; }
        // this depends on FreeRTOS systick rate as SlowTicker cannot go faster than that
        static uint32_t get_min_frequency() { return 1000; }

    private:
        static FastTicker *instance;
        FastTicker();
        virtual ~FastTicker();

        // set frequency of timer in Hz
        bool set_frequency( int frequency );

        using callback_t = std::tuple<int, uint32_t, std::function<void(void)>>;
        std::vector<callback_t> callbacks;
        uint32_t max_frequency{0};
        uint32_t interval{0}; // period in us between calls
        static bool started;
};
