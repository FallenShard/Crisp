#pragma once

#include "AbstractCamera.hpp"

namespace crisp
{
    class FreeCamera : public AbstractCamera
    {
    public:
        FreeCamera();
        ~FreeCamera();

        virtual void update(float dt) override;
        
        void walk(float dt);
        void strafe(float dt);
        void lift(float dt);

        void rotate(float dx, float dy);
        
        void setTranslation(const glm::vec3& translation);
        glm::vec3 getTranslation() const;

        void setSpeed(float speed);
        float getSpeed() const;

    protected:
        float m_speed;
        glm::vec3 m_translation;

        glm::vec3 m_yawPitchRoll;

        bool m_needsViewUpdate;
        unsigned int m_normalizationCount;
    };
}