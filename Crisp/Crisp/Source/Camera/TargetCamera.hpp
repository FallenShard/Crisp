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
        TargetCamera(RotationStrategy rotationStrategy);
        ~TargetCamera();

        virtual bool update(float dt) override;

        void setTarget(const glm::vec3& target);
        glm::vec3 getTarget() const;
        
        glm::quat getOrientation() const;

        void pan(float dx, float dy);
        void zoom(float amount);
        void move(glm::vec2 delta);

        void setZoom(float amount);

        float getDistance() const;

    private:
        glm::vec3 m_target;
        glm::vec3 m_translation; // in camera space

        glm::quat m_orientation;
        glm::vec3 m_eulerAngles;

        float m_minDistance;
        float m_maxDistance;

        static constexpr unsigned int NormalizationFrequency = 10;
        unsigned int m_normalizationCount;

        RotationStrategy m_rotationStrategy;
    };
}