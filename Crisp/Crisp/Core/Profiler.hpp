#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace crisp {
class Profiler {
public:
    Profiler()
        : m_timePoint(std::chrono::high_resolution_clock::now()) {}

    explicit Profiler(std::string sectionName)
        : m_timePoint(std::chrono::high_resolution_clock::now())
        , m_sectionName(std::move(sectionName)) {}

    ~Profiler() {
        double elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - m_timePoint).count();
        std::cout << m_sectionName << " Profiler: " << elapsed << " ms\n";
    }

    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    Profiler(Profiler&&) = delete;
    Profiler& operator=(Profiler&&) = delete;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_timePoint;
    std::string m_sectionName;
};
} // namespace crisp