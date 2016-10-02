#pragma once

#include <chrono>

namespace crisp
{
    template <typename TimeUnit>
    class Timer
    {
    public:
        Timer();
        ~Timer() = default;

        Timer(const Timer& other) = default;
        Timer& operator=(const Timer& other) = default;
        Timer& operator=(Timer&& other) = default;

        inline double getElapsedTime();
        inline double restart();

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_accumulatedTime;
    };

    template <typename TimeUnit>
    Timer<TimeUnit>::Timer()
        : m_accumulatedTime(std::chrono::high_resolution_clock::now())
    {
    }

    template <typename TimeUnit>
    double Timer<TimeUnit>::getElapsedTime()
    {
        return std::chrono::duration<double, TimeUnit>(std::chrono::high_resolution_clock::now() - m_accumulatedTime).count();
    }

    template <typename TimeUnit>
    double Timer<TimeUnit>::restart()
    {
        auto current = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, TimeUnit>(current - m_accumulatedTime).count();
        m_accumulatedTime = current;

        return elapsed;
    }
}