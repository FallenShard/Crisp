#pragma once

#include <CrispCore/Event.hpp>

namespace crisp::gui
{
class StopWatch
{
public:
    StopWatch(double triggerPeriod)
        : m_time(0.0)
        , m_triggerPeriod(triggerPeriod)
    {
    }
    ~StopWatch() {}

    void accumulate(double dt)
    {
        m_time += dt;
        if (m_time >= m_triggerPeriod)
        {
            m_time -= m_triggerPeriod;
            triggered();
        }
    }

    Event<> triggered;

private:
    double m_time;
    double m_triggerPeriod;
};
} // namespace crisp::gui