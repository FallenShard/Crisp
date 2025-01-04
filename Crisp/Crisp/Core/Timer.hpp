#pragma once

#include <chrono>

namespace crisp {
template <typename TimeUnit>
class Timer {
public:
    Timer()
        : m_prevTimePoint(std::chrono::steady_clock::now()) {}

    ~Timer() = default;

    Timer(const Timer& other) = default;
    Timer(Timer&& other) noexcept = default;
    Timer& operator=(const Timer& other) = default;
    Timer& operator=(Timer&& other) noexcept = default;

    double getElapsedTime() const {
        return TimeUnit(std::chrono::steady_clock::now() - m_prevTimePoint).count();
    }

    double restart() {
        const auto current = std::chrono::steady_clock::now();
        const double elapsed = TimeUnit(current - m_prevTimePoint).count();
        m_prevTimePoint = current;

        return elapsed;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> m_prevTimePoint;
};
} // namespace crisp