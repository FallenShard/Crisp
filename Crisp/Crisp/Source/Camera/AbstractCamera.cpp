#include "AbstractCamera.hpp"

namespace crisp
{
    AbstractCamera::AbstractCamera()
        : m_recalculateViewMatrix(true)
    {
    }

    AbstractCamera::~AbstractCamera()
    {
    }

    void AbstractCamera::setupProjection(float fovY, float aspectRatio, float zNear, float zFar)
    {
        m_fov         = glm::radians(fovY);
        m_aspectRatio = aspectRatio;
        m_zNear       = zNear;
        m_zFar        = zFar;

        m_P = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);
    }

    const glm::mat4& AbstractCamera::getProjectionMatrix() const
    {
        return m_P;
    }

    void AbstractCamera::setFov(float fovY)
    {
        m_fov = glm::radians(fovY);
        m_P = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        m_recalculateViewMatrix = true;
    }

    float AbstractCamera::getFov() const
    {
        return m_fov;
    }

    void AbstractCamera::setApectRatio(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
        m_P = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        m_recalculateViewMatrix = true;
    }

    float AbstractCamera::getAspectRatio() const
    {
        return m_aspectRatio;
    }

    void AbstractCamera::setNearPlaneDistance(float zNear)
    {
        m_zNear = zNear;
        m_P = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        m_recalculateViewMatrix = true;
    }

    float AbstractCamera::getNearPlaneDistance() const
    {
        return m_zNear;
    }

    void AbstractCamera::setFarPlaneDistance(float zFar)
    {
        m_zFar = zFar;
        m_P = glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);

        m_recalculateViewMatrix = true;
    }

    float AbstractCamera::getFarPlaneDistance() const
    {
        return m_zFar;
    }

    void AbstractCamera::setPosition(const glm::vec3& position)
    {
        m_position = position;
        m_recalculateViewMatrix = true;
    }

    glm::vec3 AbstractCamera::getPosition() const
    {
        return m_position;
    }

    glm::vec3 AbstractCamera::getLookDirection() const
    {
        return m_look;
    }
    
    glm::vec3 AbstractCamera::getRightDirection() const
    {
        return m_right;
    }

    glm::vec3 AbstractCamera::getUpDirection() const
    {
        return m_up;
    }

    const glm::mat4& AbstractCamera::getViewMatrix() const
    {
        return m_V;
    }

    void AbstractCamera::calculateFrustumPlanes() const
    {
    }

    bool AbstractCamera::isPointInFrustum(const glm::vec3& point)
    {
        return false;
    }

    bool AbstractCamera::isSphereInFrustum(const glm::vec3& center, float radius)
    {
        return false;
    }

    bool AbstractCamera::isBoxInFrustum(const glm::vec3& min, const glm::vec3& max)
    {
        return false;
    }

    std::array<glm::vec4, AbstractCamera::NumFrustumPlanes> AbstractCamera::getFrustumPlanes() const
    {
        return m_frustumPlanes;
    }

    glm::vec4 AbstractCamera::getFrustumPlane(size_t index) const
    {
        return m_frustumPlanes.at(index);
    }
}