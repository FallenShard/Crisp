#include "TargetCamera.hpp"

namespace crisp
{
    namespace
    {
        glm::vec3 panTranslation = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    TargetCamera::TargetCamera(RotationStrategy /*rotationStrategy*/)
        : m_rotationStrategy(RotationStrategy::IncrementalQuaternions)
    {
        m_fovY        = glm::radians(45.0f);
        m_aspectRatio = 1.0f;
        m_zNear       = 0.5f;
        m_zFar        = 100.0f;
        //m_P           = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        m_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        m_position = glm::vec3(5.0f, 5.0f, 5.0f);
        m_target   = glm::vec3(0.0f, 0.0f, 0.0f);
        m_look     = glm::normalize(m_target - m_position);
        m_right    = glm::vec3(1.0f, 0.0f, 0.0f);
        m_up       = glm::vec3(0.0f, 1.0f, 0.0f);
        m_V        = glm::lookAt(m_position, m_target, m_up);

        m_translation   = glm::vec3(0.0f);
        m_distance      = glm::distance(m_position, m_target);
        m_minDistance   = 1.0f;
        m_maxDistance   = 100.0f;

        m_absPos = m_position + m_translation;
        m_absTarget = m_target + m_translation;

        m_normalizationCount = 0;
    }

    TargetCamera::~TargetCamera()
    {
    }

    void TargetCamera::update(float /*dt*/)
    {
        if (++m_normalizationCount == NormalizationFrequency)
        {
            m_orientation        = glm::normalize(m_orientation);
            m_normalizationCount = 0;
        }

        // View matrix rotation component
        const glm::mat4 rotation = m_rotationStrategy == RotationStrategy::EulerAngles ?
            glm::yawPitchRoll(m_eulerAngles.x, m_eulerAngles.y, m_eulerAngles.z) :
            glm::mat4_cast(m_orientation);

        // Camera position in world space, obtained by rotating its translation
        m_position = glm::vec3(rotation * glm::vec4(0.0f, 0.0f, m_distance, 1.0f));

        m_look  = glm::vec3(-rotation[2]);
        m_up    = glm::vec3(rotation[1]);
        m_right = glm::cross(m_look, m_up);
        m_V     = glm::lookAt(m_position + m_translation, m_target + m_translation, m_up);
    }

    void TargetCamera::rotate(float dx, float dy)
    {
        if (m_rotationStrategy == RotationStrategy::EulerAngles)
        {
            m_eulerAngles.x += dx;
            m_eulerAngles.y += dy;
        }
        else
        {
            glm::quat quat(glm::vec3(dy, dx, 0.0f));
            m_orientation *= quat;
        }
    }

    void TargetCamera::walk(float dt)
    {
        m_translation += m_look * dt;
    }

    void TargetCamera::strafe(float dt)
    {
        m_translation += m_right * dt;
    }

    void TargetCamera::lift(float /*dt*/)
    {
    }

    void TargetCamera::setTarget(const glm::vec3& target)
    {
        m_target = target;
    }

    glm::vec3 TargetCamera::getTarget() const
    {
        return m_target;
    }

    glm::quat TargetCamera::getOrientation() const
    {
        return m_orientation;
    }

    void TargetCamera::zoom(float amount)
    {
        m_distance = glm::clamp(m_distance + amount, m_minDistance, m_maxDistance);
    }

    void TargetCamera::setZoom(float zoom)
    {
        m_distance = glm::clamp(zoom, m_minDistance, m_maxDistance);
    }

    float TargetCamera::getDistance() const
    {
        return m_distance;
    }
}