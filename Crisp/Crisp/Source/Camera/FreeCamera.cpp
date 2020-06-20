#include "FreeCamera.hpp"

#include <iostream>

namespace crisp
{
    namespace
    {
        unsigned int NormalizationFrequency = 10;

        std::ostream& operator<<(std::ostream& out, const glm::vec3& v)
        {
            out << "[" << v.x << ", " << v.y << ", " << v.z << "]\n";
            return out;
        }
    }

    FreeCamera::FreeCamera()
    {
        m_fov         = glm::radians(45.0f);
        m_aspectRatio = 1.0f;
        m_zNear       = 1.0f;
        m_zFar        = 100.0f;
        //m_P           = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        //m_yawPitchRoll = glm::radians(glm::vec3(45.0f, -30.0f, 0.0f));
        m_yawPitchRoll = glm::radians(glm::vec3(0.0f));

        m_position = glm::vec3(0.0, 0.0f, 20.0f);
        m_look     = glm::vec3(0.0f, 0.0f, -1.0f);
        m_right    = glm::vec3(1.0f, 0.0f, 0.0f);
        m_up       = glm::vec3(0.0f, 1.0f, 0.0f);
        m_V        = glm::lookAt(m_position, glm::vec3(0.0f), m_up);

        m_translation = glm::vec3(0.0f);

        m_recalculateViewMatrix = true;
        m_normalizationCount = 0;

        m_speed = 3.0f;
    }

    FreeCamera::~FreeCamera()
    {
    }

    bool FreeCamera::update(float /*dt*/)
    {
        if (!m_recalculateViewMatrix)
            return false;

        m_recalculateViewMatrix = false;

        auto rotation = glm::yawPitchRoll(m_yawPitchRoll.x, m_yawPitchRoll.y, m_yawPitchRoll.z);

        m_position += m_translation * m_speed;
        m_translation = glm::vec3(0.0f);

        m_look  = glm::vec3(-rotation[2]);
        m_up    = glm::vec3(rotation[1]);
        m_right = glm::cross(m_look, m_up);
        m_V = glm::transpose(rotation) * glm::translate(-m_position);

        calculateFrustumPlanes();

        return true;
    }

    void FreeCamera::rotate(float dx, float dy)
    {
        m_yawPitchRoll.x += dx;
        m_yawPitchRoll.y += dy;

        m_recalculateViewMatrix = true;
    }

    void FreeCamera::walk(float dt)
    {
        m_translation += m_look * dt;
        m_recalculateViewMatrix = true;
    }

    void FreeCamera::strafe(float dt)
    {
        m_translation += m_right * dt;
        m_recalculateViewMatrix = true;
    }

    void FreeCamera::lift(float dt)
    {
        m_translation += m_up * dt;
        m_recalculateViewMatrix = true;
    }

    void FreeCamera::setTranslation(const glm::vec3& translation)
    {
        m_translation = translation;
    }

    glm::vec3 FreeCamera::getTranslation() const
    {
        return m_translation;
    }

    void FreeCamera::setSpeed(float speed)
    {
        m_speed = speed;
    }

    float FreeCamera::getSpeed() const
    {
        return m_speed;
    }
}