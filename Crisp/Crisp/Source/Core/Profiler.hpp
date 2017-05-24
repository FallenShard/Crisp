#pragma once

#include <chrono>
#include <iostream>

namespace crisp
{
    class Profiler
    {
    public:
        Profiler()
        {
            m_timePoint = std::chrono::high_resolution_clock::now();
        }

        ~Profiler()
        {
            double elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - m_timePoint).count();
            std::cout << "Profiler: " << elapsed << " ms\n";
        }

        static void start()
        {
            TimePoint = std::chrono::high_resolution_clock::now();
        }

        static void end()
        {
            double elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - TimePoint).count();
            std::cout << "Profiler: " << elapsed << " ms\n";
        }

    private:
        static std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

        std::chrono::time_point<std::chrono::high_resolution_clock> m_timePoint;
    };

    std::chrono::time_point<std::chrono::high_resolution_clock> Profiler::TimePoint;
}