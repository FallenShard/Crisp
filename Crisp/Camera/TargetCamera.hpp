#pragma once

#include "AbstractCamera.hpp"

namespace crisp
{
    enum class RotationStrategy
    {
        EulerAngles,
        IncrementalQuaternions
    };

    class TargetCamera : public AbstractCamera
    {
    public:
        TargetCamera(RotationStrategy rotationStrategy = RotationStrategy::EulerAngles);
        ~TargetCamera();

        virtual void update(float dt) override;
        virtual void rotate(float dx, float dy) override;

        virtual void walk(float dt) override;
        virtual void strafe(float dt) override;
        virtual void lift(float dt) override;

        virtual void setTarget(const glm::vec3& target) override;
        glm::vec3 getTarget() const;

        glm::quat getOrientation() const;

        void zoom(float amount);

        void setZoom(float amount);

        float getDistance() const;

    private:
        glm::vec3 m_target;
        glm::vec3 m_translation; // in world space

        glm::quat m_orientation;
        glm::vec3 m_eulerAngles;

        float m_distance;
        float m_minDistance;
        float m_maxDistance;

        static constexpr unsigned int NormalizationFrequency = 10;
        unsigned int m_normalizationCount;

        RotationStrategy m_rotationStrategy;
    };
}