#pragma once

#include "AbstractCamera.hpp"

namespace crisp
{
    class TargetCamera : public AbstractCamera
    {
    public:
        TargetCamera();
        ~TargetCamera();

        virtual void update(float dt) override;

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

        float m_minDistance;
        float m_maxDistance;

        bool m_needsViewUpdate;
        unsigned int m_normalizationCount;
    };
}