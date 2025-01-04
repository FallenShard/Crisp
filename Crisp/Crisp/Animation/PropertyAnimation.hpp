#pragma once

#include <Crisp/Animation/Animation.hpp>

#include <Crisp/Math/Constants.hpp>

namespace crisp {
enum class Easing : uint8_t {
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

template <typename T, Easing easing = Easing::Linear>
class PropertyAnimation : public Animation {
public:
    PropertyAnimation(double duration, T start, T end, double delay = 0, bool isLooped = false, int loopCount = 1)
        : Animation(delay, duration, isLooped, loopCount)
        , m_propertyStart(start)
        , m_propertyEnd(end) {}

    PropertyAnimation(
        double duration,
        T start,
        T end,
        std::function<void(const T&)> updater,
        double delay = 0,
        bool isLooped = false,
        int loopCount = 1)
        : Animation(delay, duration, isLooped, loopCount)
        , m_propertyStart(start)
        , m_propertyEnd(end)
        , m_updater(updater) {}

    PropertyAnimation(
        double duration, std::function<void(const T&)> updater, double delay = 0, bool isLooped = false, int loopCount = 1)
        : Animation(delay, duration, isLooped, loopCount)
        , m_updater(updater) {}

    T getPropertyStart() {
        return m_propertyStart;
    }

    T getPropertyEnd() {
        return m_propertyEnd;
    }

    void setUpdater(std::function<void(const T&)> updater) {
        m_updater = updater;
    }

    void update(double dt) override {
        if (m_isFinished) {
            return;
        }

        if (m_elapsedDelayTime < m_startDelay) {
            m_elapsedDelayTime += dt;
            return;
        }

        if (m_elapsedTime < m_duration) {
            m_elapsedTime += dt;
            const double frameTime = hasElapsed() ? 1.0 : interpolateTime(m_elapsedTime / m_duration);

            if (m_updater) {
                const T lerpVal = m_propertyStart + (m_propertyEnd - m_propertyStart) * static_cast<float>(frameTime);
                m_updater(lerpVal);
            }

            m_framesCompleted++;
        }

        if (hasElapsed()) {
            m_loopsCompleted++;

            if (m_isLooped && m_loopsCompleted < m_loopCount) {
                m_isFinished = false;
                m_elapsedTime -= m_duration;
            } else {
                m_isFinished = true;
                finished();
            }
        }
    }

    bool hasElapsed() const {
        return std::abs(m_elapsedTime - m_duration) <= 1e-7;
    }

    void reset(T start, T end) {
        m_propertyStart = start;
        m_propertyEnd = end;
        Animation::reset();
    }

private:
    double interpolateTime(double dt) {
        if constexpr (easing == Easing::QuadraticIn) {
            return dt * dt;
        } else if constexpr (easing == Easing::CubicIn) {
            return dt * dt * dt;
        } else if constexpr (easing == Easing::QuarticIn || easing == Easing::SlowIn) {
            return dt * dt * dt * dt;
        } else if constexpr (easing == Easing::QuadraticOut) {
            return -1.0 * dt * (dt - 2.0);
        } else if constexpr (easing == Easing::CubicOut) {
            dt -= 1.0;
            return dt * dt * dt + 1.0;
        } else if constexpr (easing == Easing::QuarticOut || easing == Easing::SlowOut) {
            dt -= 1.0;
            dt *= dt;
            return -1.0 * (dt * dt - 1.0);
        } else if constexpr (easing == Easing::Root) {
            return std::sqrt(dt);
        } else if constexpr (easing == Easing::Sine) {
            return std::sin(dt * PI<double> / 2.0);
        } else {
            return dt;
        }
    }

    T m_propertyStart;
    T m_propertyEnd;

    std::function<void(const T&)> m_updater;
};
} // namespace crisp