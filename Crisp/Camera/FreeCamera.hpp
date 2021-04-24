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
        virtual void rotate(float dx, float dy) override;
        virtual void setRotation(float yaw, float pitch, float roll) override;

        virtual void walk(float dt) override;
        virtual void strafe(float dt) override;
        virtual void lift(float dt) override;

        virtual void setTarget(const glm::vec3& target) override;

        void setTranslation(const glm::vec3& translation);
        glm::vec3 getTranslation() const;

        void setSpeed(float speed);
        float getSpeed() const;

    protected:
        float m_speed;
        glm::vec3 m_translation;

        glm::vec3 m_yawPitchRoll;

        unsigned int m_normalizationCount;
    };
}