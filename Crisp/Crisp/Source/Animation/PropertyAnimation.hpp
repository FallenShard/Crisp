#pragma once

#include "Animation.hpp"

namespace crisp
{
    enum class Easing
    {
        Linear,
        SlowIn,
        SlowOut,
        QuadraticIn,
        CubicIn,
        QuarticIn,
        QuadraticOut,
        CubicOut,
        QuarticOut,
        Root,
        Sine
    };

    template <typename T>
    class PropertyAnimation : public Animation
    {
    public:
        PropertyAnimation(double duration, T start, T end, double delay = 0, Easing easing = Easing::Linear, bool isLooped = false, int loopCount = 1)
            : Animation(delay, duration, isLooped, loopCount)
            , m_propertyStart(start)
            , m_propertyEnd(end)
            , m_easing(easing)
        {
            createInterpolator();
        }

        inline T getPropertyStart()
        {
            return m_propertyStart;
        }

        inline T getPropertyEnd()
        {
            return m_propertyEnd;
        }

        inline void setEasing(Easing easing)
        {
            m_easing = easing;
            m_interpolator = createInterpolator(easing);
        }

        void setUpdater(std::function<void(const T&)> updater)
        {
            m_updater = updater;
        }

        virtual void update(double dt) override
        {
            if (m_isFinished)
                return;

            if (m_elapsedDelayTime < m_startDelay)
            {
                m_elapsedDelayTime += dt;
                return;
            }

            if (m_elapsedTime < m_duration)
            {
                double frameTime = m_interpolator(m_elapsedTime / m_duration);

                if (m_updater)
                {
                    T lerpVal = m_propertyStart + (m_propertyEnd - m_propertyStart) * (float)frameTime;
                    m_updater(lerpVal);
                }

                m_elapsedTime += dt;
                m_frameCount++;
            }
            else
            {
                if (m_updater)
                    m_updater(m_propertyEnd);
                m_loopsCompleted++;

                if (m_isLooped && m_loopsCompleted < m_loopCount)
                {
                    m_isFinished = false;
                    m_elapsedTime = 0.0;
                }
                else
                {
                    m_isFinished = true;
                    finished();
                }
            }
        }

    private:
        void createInterpolator()
        {
            switch (m_easing)
            {
            case Easing::Linear:
                m_interpolator = [](double dt) { return dt; };
                break;

            case Easing::QuadraticIn:
                m_interpolator = [](double dt) { return dt * dt; };
                break;

            case Easing::CubicIn:
                m_interpolator = [](double dt) { return dt * dt * dt; };
                break;

            case Easing::SlowIn:
            case Easing::QuarticIn:
                m_interpolator = [](double dt) { return dt * dt * dt * dt; };
                break;

            case Easing::QuadraticOut:
                m_interpolator = [](double dt)
                {
                    dt -= 1.0;
                    return -1.0 * (dt * dt - 1.0);
                };
                break;

            case Easing::CubicOut:
                m_interpolator = [](double dt)
                {
                    dt -= 1.0;
                    return -1.0 * (dt * dt * dt - 1.0);
                };
                break;

            case Easing::SlowOut:
            case Easing::QuarticOut:
                m_interpolator = [](double dt)
                {
                    dt -= 1.0;
                    dt *= dt;
                    return -1.0 * (dt * dt - 1.0);
                };
                break;

            case Easing::Root:
                m_interpolator = [](double dt) { return std::sqrt(dt); };
                break;

            case Easing::Sine:
                m_interpolator = [](double dt) { return std::sin(dt * 3.141592 / 2.0); };
                break;

            default:
                m_interpolator = [](double dt) { return dt; };
                break;
            }
        }


        T m_propertyStart;
        T m_propertyEnd;

        Easing m_easing;
        std::function<double(double)> m_interpolator;

        std::function<void(const T&)> m_updater;
    };
}