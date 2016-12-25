#include "FreeCamera.hpp"

#include <glm/gtx/euler_angles.hpp>

namespace crisp
{
    FreeCamera::FreeCamera()
    {
        m_position = glm::vec3(5.0f, 5.0f, 5.0f);
        auto target = glm::vec3(0.0f, 0.0f, 0.0f);
        m_up = glm::vec3(0.0f, 1.0f, 0.0f);
        m_right = glm::cross(glm::normalize(target - m_position), m_up);
        m_V = glm::lookAt(m_position, target, m_up);
    }

    FreeCamera::~FreeCamera()
    {
    }

    void FreeCamera::update(float dt)
    {
        auto rotation = glm::mat4(1.0f);// glm::yawPitchRoll(m_yawPitchRoll.x, m_yawPitchRoll.y, m_yawPitchRoll.z);
        
        m_position += m_translation;
        m_translation = glm::vec3(0.0f);
        
        m_look  = glm::vec3(rotation * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
        m_up    = glm::vec3(rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
        m_right = glm::cross(m_look, m_up);
        
        auto target = m_position + m_look;
        m_V = glm::lookAt(m_position, target, m_up);
    }

    void FreeCamera::walk(float dt)
    {
        m_translation = m_look * dt;
    }

    void FreeCamera::strafe(float dt)
    {
        m_translation = m_right * dt;
    }

    void FreeCamera::lift(float dt)
    {
        m_translation = m_up * dt;
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