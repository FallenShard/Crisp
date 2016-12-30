#include "TargetCamera.hpp"

#include <glm/gtx/euler_angles.hpp>

namespace crisp
{
    namespace
    {
        unsigned int NormalizationFrequency = 10;
    }

    TargetCamera::TargetCamera()
    {
        m_fov         = glm::radians(45.0f);
        m_aspectRatio = 1.0f;
        m_zNear       = 0.5f;
        m_zFar        = 100.0f;
        m_P           = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        m_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        m_position = glm::vec3(0.0f, 0.0f, 5.0f);
        m_target   = glm::vec3(0.0f, 0.0f, 0.0f);
        m_look     = glm::normalize(m_target - m_position);
        m_right    = glm::vec3(1.0f, 0.0f, 0.0f);
        m_up       = glm::vec3(0.0f, 1.0f, 0.0f);
        m_V        = glm::lookAt(m_position, m_target, m_up);

        m_translation   = glm::vec3(0.0f);
        m_translation.z = glm::distance(m_position, m_target);
        m_minDistance   = 1.0f;
        m_maxDistance   = 10.0f;

        m_needsViewUpdate = true;
        m_normalizationCount = 0;
    }

    TargetCamera::~TargetCamera()
    {
    }

    void TargetCamera::update(float dt)
    {
        if (!m_needsViewUpdate)
            return;

        if (++m_normalizationCount == NormalizationFrequency)
        {
            m_orientation = glm::normalize(m_orientation);
            m_normalizationCount = 0;
        }

        auto rotation = glm::mat4_cast(m_orientation);

        auto rotatedTrans = glm::vec3(rotation * glm::vec4(m_translation, 0.0f));

        m_position = rotatedTrans;
        m_look     = glm::normalize(m_target - m_position);
        m_up       = glm::vec3(rotation[1]);
        m_right    = glm::cross(m_look, m_up);
        m_V        = glm::lookAt(m_position, m_target, m_up);

        m_needsViewUpdate = false;
    }

    void TargetCamera::setTarget(const glm::vec3& target)
    {
        m_target = target;
        m_needsViewUpdate = true;
    }

    glm::vec3 TargetCamera::getTarget() const
    {
        return m_target;
    }

    glm::quat TargetCamera::getOrientation() const
    {
        return m_orientation;
    }

    void TargetCamera::pan(float dx, float dy)
    {
        m_translation.x += dx;
        m_translation.y += dy;

        m_target.x += dx;
        m_target.y += dy;


        m_needsViewUpdate = true;
    }

    void TargetCamera::zoom(float amount)
    {
        m_position += m_look * amount;
        m_translation.z = glm::distance(m_position, m_target);
        m_translation.z = glm::clamp(m_translation.z, m_minDistance, m_maxDistance);
        m_needsViewUpdate = true;
    }

    void TargetCamera::setZoom(float zoom)
    {
        m_position = m_look * zoom;
        m_translation.z = glm::distance(m_position, m_target);
        m_translation.z = glm::clamp(m_translation.z, m_minDistance, m_maxDistance);
        m_needsViewUpdate = true;
    }

    void TargetCamera::move(glm::vec2 delta)
    {
        glm::quat quat(glm::vec3(delta.y, delta.x, 0.0f));

        m_orientation *= quat;
        m_needsViewUpdate = true;
    }

    float TargetCamera::getDistance() const
    {
        return m_translation.z;
    }
}