#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>

#include "Core/Event.hpp"

namespace crisp
{
    template <typename Timer>
    class FrameTimeLogger : Timer
    {
    public:
        FrameTimeLogger(double msUpdatePeriod = 250.0);

        void start();
        double restart();

        Event<void, const std::string&> onFpsUpdated;

    private:
        double m_accumulatedTime;
        double m_accumulatedFrames;
        double m_updatePeriod;
    };

    template <typename Timer>
    FrameTimeLogger<Timer>::FrameTimeLogger(double msUpdatePeriod = 250.0)
        : m_accumulatedTime(0.0)
        , m_accumulatedFrames(0.0)
        , m_updatePeriod(msUpdatePeriod)
    {
    }

    template <typename Timer>
    void FrameTimeLogger<Timer>::start()
    {
        Timer::restart();
    }

    template <typename Timer>
    double FrameTimeLogger<Timer>::restart()
    {
        double frameTime = Timer::restart();
        m_accumulatedTime += frameTime;
        m_accumulatedFrames++;

        if (m_accumulatedTime >= m_updatePeriod)
        {
            double spillOver = m_accumulatedTime - m_updatePeriod;
            double spillOverFrac = spillOver / frameTime;

            double avgMillis = m_updatePeriod / (m_accumulatedFrames - spillOverFrac);
            double avgFrames = 1000.0 / avgMillis;

            std::stringstream stringStream;
            stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << avgMillis << " ms, " << std::setfill('0') << avgFrames << " fps";
            onFpsUpdated(stringStream.str());

            m_accumulatedTime = spillOver;
            m_accumulatedFrames = spillOverFrac;
        }
        start();

        return frameTime;
    }
}