#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

#include <CrispCore/Event.hpp>

namespace crisp
{
    namespace internal
    {
        template <typename T> struct StringRepresentation;
        template <>           struct StringRepresentation<std::nano>  { static constexpr const char* value = "ns"; };
        template <>           struct StringRepresentation<std::micro> { static constexpr const char* value = "us"; };
        template <>           struct StringRepresentation<std::milli> { static constexpr const char* value = "ms"; };
    }

    template <typename Timer>
    class FrameTimeLogger : public Timer
    {
    public:
        FrameTimeLogger(double msUpdatePeriod = 250.0);
        ~FrameTimeLogger() = default;

        void update();

        Event<double, double> onLoggerUpdated;

    private:
        double m_accumulatedTime;
        double m_accumulatedFrames;
        double m_updatePeriod;
    };

    template <typename Timer>
    FrameTimeLogger<Timer>::FrameTimeLogger(double msUpdatePeriod)
        : m_accumulatedTime(0.0)
        , m_accumulatedFrames(0.0)
        , m_updatePeriod(msUpdatePeriod)
    {
    }

    template <typename Timer>
    void FrameTimeLogger<Timer>::update()
    {
        double frameTime = Timer::restart();
        m_accumulatedTime += frameTime;
        m_accumulatedFrames++;

        if (m_accumulatedTime >= m_updatePeriod)
        {
            double spillOver = m_accumulatedTime - m_updatePeriod;
            double spillOverFrac = spillOver / frameTime;

            double avgMillis = m_updatePeriod / (m_accumulatedFrames - spillOverFrac);
            double avgFrames = static_cast<double>(Timer::TimeRatio::den) / avgMillis;

            onLoggerUpdated(avgMillis, avgFrames);

            m_accumulatedTime = spillOver;
            m_accumulatedFrames = spillOverFrac;
        }
    }
}