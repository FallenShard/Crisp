#pragma once

#include <CrispCore/Event.hpp>
#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
class Animation
{
public:
    Animation(double startDelay, double duration, bool isLooped = false, int loopCount = 1);
    virtual ~Animation() = default;

    double getDuration() const;

    double getStartDelay() const;

    double getElapsedTime() const;
    bool isFinished() const;

    void setActive(bool active);
    bool isActive() const;

    virtual void update(double dt) = 0;

    virtual void reset();

    Event<> started;
    Event<> finished;

protected:
    double m_duration;
    double m_startDelay;
    double m_elapsedTime;
    double m_elapsedDelayTime;

    bool m_isActive;

    bool m_isFinished;
    bool m_isLooped;
    int m_loopCount;

    int m_loopsCompleted;
    int m_framesCompleted;
};
} // namespace crisp