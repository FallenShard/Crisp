#pragma once

#include <functional>

#include "Math/Headers.hpp"
#include "Core/Event.hpp"

namespace crisp
{
    class Animation
    {
    public:
        Animation(double startDelay, double duration, bool isLooped = false, int loopCount = 1);
        ~Animation();

        Event<void> started;
        Event<void> finished;

        void setDuration(double duration);
        double getDuration() const;

        void setStartDelay(double delay);
        double getStartDelay() const;

        double getElapsedTime() const;
        bool isFinished() const;

        virtual void update(double dt) = 0;

        virtual void reset();

    protected:
        double m_duration;
        double m_startDelay;
        double m_elapsedTime;
        double m_elapsedDelayTime;
        bool m_isFinished;
        bool m_isLooped;
        int m_loopCount;

        int m_loopsCompleted;
        int m_frameCount;

    };
}