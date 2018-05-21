#include "Animation.hpp"
#include "Animator.hpp"

namespace crisp
{
    namespace
    {
        static int Id = 0;
    }

    Animation::Animation(double startDelay, double duration, bool isLooped, int loopCount)
        : m_startDelay(startDelay)
        , m_duration(duration)
        , m_isLooped(isLooped)
        , m_isActive(false)
        , m_isFinished(false)
        , m_loopCount(loopCount == 0 ? std::numeric_limits<int>::max() : loopCount)
        , m_loopsCompleted(0)
        , m_elapsedTime(0)
        , m_elapsedDelayTime(0)
    {
    }

    Animation::~Animation()
    {
    }

    void Animation::setDuration(double duration)
    {
        m_duration = duration;
    }

    double Animation::getDuration() const
    {
        return m_duration;
    }

    void Animation::setStartDelay(double delay)
    {
        m_startDelay = delay;
    }

    double Animation::getStartDelay() const
    {
        return m_startDelay;
    }

    double Animation::getElapsedTime() const
    {
        return m_elapsedTime;
    }

    bool Animation::isFinished() const
    {
        return m_isFinished;
    }

    void Animation::setActive(bool active)
    {
        m_isActive = active;
    }

    bool Animation::isActive() const
    {
        return m_isActive;
    }

    void Animation::reset()
    {
        m_loopsCompleted = 0;
        m_elapsedDelayTime = 0;
        m_elapsedTime = 0;
        m_isFinished = false;
    }
}