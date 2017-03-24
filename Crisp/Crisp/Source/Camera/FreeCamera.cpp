#include "FreeCamera.hpp"

#include <glm/gtx/euler_angles.hpp>

namespace crisp
{
    namespace
    {
        unsigned int NormalizationFrequency = 10;
    }

    FreeCamera::FreeCamera()
    {
        m_fov         = glm::radians(45.0f);
        m_aspectRatio = 1.0f;
        m_zNear       = 0.5f;
        m_zFar        = 100.0f;
        m_P           = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        m_yawPitchRoll = glm::vec3(180.0f, 0.0f, 0.0f);

        m_position = glm::vec3(0.0f, 0.0f, 5.0f);
        m_look     = glm::vec3(0.0f, 0.0f, -1.0f);
        m_right    = glm::vec3(1.0f, 0.0f, 0.0f);
        m_up       = glm::vec3(0.0f, 1.0f, 0.0f);
        m_V        = glm::lookAt(m_position, glm::vec3(0.0f), m_up);

        m_translation = glm::vec3(0.0f);

        m_needsViewUpdate = true;
        m_normalizationCount = 0;
    }

    FreeCamera::~FreeCamera()
    {
    }

    bool FreeCamera::update(float dt)
    {
        if (!m_needsViewUpdate)
            return false;
        
        auto rotation = glm::yawPitchRoll(m_yawPitchRoll.x, m_yawPitchRoll.y, m_yawPitchRoll.z);
        
        m_position += m_translation;
        m_translation = glm::vec3(0.0f);
        
        m_look  = glm::vec3(rotation[2]);
        m_up    = glm::vec3(rotation[1]);
        m_right = glm::cross(m_up, m_look);
        
        auto target = m_position + m_look;
        m_V = rotation * glm::translate(-m_position);
        
        m_needsViewUpdate = false;
        return true;
    }

    void FreeCamera::walk(float dt)
    {
        m_translation = m_look * dt;
        m_needsViewUpdate = true;
    }

    void FreeCamera::strafe(float dt)
    {
        m_translation = m_right * dt;
        m_needsViewUpdate = true;
    }

    void FreeCamera::lift(float dt)
    {
        m_translation = m_up * dt;
        m_needsViewUpdate = true;
    }

    void FreeCamera::rotate(float dx, float dy)
    {
        m_yawPitchRoll.x += dx;
        m_yawPitchRoll.y += dy;

        m_needsViewUpdate = true;
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