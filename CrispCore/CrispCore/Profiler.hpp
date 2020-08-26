#pragma once

#include <chrono>
#include <iostream>
#include <string>

namespace crisp
{
    class Profiler
    {
    public:
        Profiler()
            : m_timePoint(std::chrono::high_resolution_clock::now())
        {
        }

        Profiler(const std::string& sectionName)
            : m_sectionName(sectionName)
            , m_timePoint(std::chrono::high_resolution_clock::now())
        {
        }

        ~Profiler()
        {
            double elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - m_timePoint).count();
            std::cout << m_sectionName << " Profiler: " << elapsed << " ms\n";
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_timePoint;
        std::string m_sectionName;
    };
}