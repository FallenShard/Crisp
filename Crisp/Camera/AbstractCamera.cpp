#include "AbstractCamera.hpp"

namespace crisp
{
    namespace
    {
        static const glm::mat4 invertProjectionY = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));

        glm::mat4 reverseZPerspective(float fovY, float aspectRatio, float zNear, float zFar)
        {
            const glm::mat4 test = glm::perspective(fovY, aspectRatio, zNear, 500.0f);
            const float f = 1.0f / std::tan(fovY / 2.0f);
            //return glm::mat4(
            //    f / aspectRatio, 0.0f,  0.0f,  0.0f,
            //               0.0f,    f,  0.0f,  0.0f,
            //               0.0f, 0.0f,  -zFar / (zNear - zFar) - 1, -1.0f,
            //               0.0f, 0.0f, -zNear * zFar / (zNear - zFar),  0.0f);

            // Infinite projection
            return glm::mat4(
                f / aspectRatio, 0.0f,  0.0f,  0.0f,
                           0.0f,    f,  0.0f,  0.0f,
                           0.0f, 0.0f,  0.0f, -1.0f,
                           0.0f, 0.0f, zNear,  0.0f);
        }
    }

    AbstractCamera::AbstractCamera()
        : m_aspectRatio(1.0f)
        , m_fovY(45.0f)
        , m_zNear(1.0f)
        , m_zFar(100.0f)
    {
    }

    void AbstractCamera::setupProjection(float fovY, float aspectRatio, float zNear, float zFar)
    {
        m_fovY        = glm::radians(fovY);
        m_aspectRatio = aspectRatio;
        m_zNear       = zNear;
        m_zFar        = zFar;

        updateProjectionMatrix();
    }

    void AbstractCamera::setupProjection(float fovY, float aspectRatio)
    {
        m_fovY        = glm::radians(fovY);
        m_aspectRatio = aspectRatio;

        updateProjectionMatrix();
    }

    const glm::mat4& AbstractCamera::getProjectionMatrix() const
    {
        return m_P;
    }

    void AbstractCamera::setFovY(float fovY)
    {
        m_fovY = glm::radians(fovY);
        updateProjectionMatrix();
    }

    float AbstractCamera::getFovY() const
    {
        return glm::degrees(m_fovY);
    }

    void AbstractCamera::setApectRatio(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
        updateProjectionMatrix();
    }

    float AbstractCamera::getAspectRatio() const
    {
        return m_aspectRatio;
    }

    void AbstractCamera::setNearPlaneDistance(float zNear)
    {
        m_zNear = zNear;
        updateProjectionMatrix();
    }

    float AbstractCamera::getNearPlaneDistance() const
    {
        return m_zNear;
    }

    void AbstractCamera::setFarPlaneDistance(float zFar)
    {
        m_zFar = zFar;
        updateProjectionMatrix();
    }

    float AbstractCamera::getFarPlaneDistance() const
    {
        return m_zFar;
    }

    void AbstractCamera::setPosition(const glm::vec3& position)
    {
        m_position = position;
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

    bool AbstractCamera::isPointInFrustum(const glm::vec3& /*point*/)
    {
        return false;
    }

    bool AbstractCamera::isSphereInFrustum(const glm::vec3& /*center*/, float /*radius*/)
    {
        return false;
    }

    bool AbstractCamera::isBoxInFrustum(const glm::vec3& /*min*/, const glm::vec3& /*max*/)
    {
        return false;
    }

    std::array<glm::vec4, AbstractCamera::FrustumPlaneCount> AbstractCamera::getFrustumPlanes() const
    {
        return m_frustumPlanes;
    }

    glm::vec4 AbstractCamera::getFrustumPlane(size_t index) const
    {
        return m_frustumPlanes.at(index);
    }

    std::array<glm::vec3, AbstractCamera::FrustumPointCount> AbstractCamera::getFrustumPoints(float zNear, float zFar) const
    {
        const float tanFovHalf = std::tan(m_fovY / 2.0f);

        const float halfNearH = zNear * tanFovHalf;
        const float halfNearW = halfNearH * m_aspectRatio;

        const float halfFarH = zFar * tanFovHalf;
        const float halfFarW = halfFarH * m_aspectRatio;

        std::array<glm::vec3, AbstractCamera::FrustumPointCount> frustumPoints =
        {
            glm::vec3(-halfNearW, -halfNearH, -zNear),
            glm::vec3(+halfNearW, -halfNearH, -zNear),
            glm::vec3(+halfNearW, +halfNearH, -zNear),
            glm::vec3(-halfNearW, +halfNearH, -zNear),
            glm::vec3(-halfFarW, -halfFarH, -zFar),
            glm::vec3(+halfFarW, -halfFarH, -zFar),
            glm::vec3(+halfFarW, +halfFarH, -zFar),
            glm::vec3(-halfFarW, +halfFarH, -zFar)
        };

        const glm::mat4 camToWorld = glm::inverse(m_V);
        for (auto& p : frustumPoints)
            p = glm::vec3(camToWorld * glm::vec4(p, 1.0f));

        return frustumPoints;
    }

    std::array<glm::vec3, AbstractCamera::FrustumPointCount> AbstractCamera::getFrustumPoints() const
    {
        return getFrustumPoints(m_zNear, m_zFar);
    }

    glm::vec4 AbstractCamera::calculateBoundingSphere(float n, float f) const
    {
        const float k = std::sqrt(1.0f + m_aspectRatio * m_aspectRatio) * std::tan(m_fovY / 2.0f);
        if (k * k >= (f - n) / (f + n))
        {
            const glm::vec3 center = glm::inverse(m_V) * glm::vec4(0.0f, 0.0f, -f, 1.0f);
            return glm::vec4(center, f * k);
        }
        else
        {
            const glm::vec3 center = glm::inverse(m_V) * glm::vec4(0.0f, 0.0f, -0.5f * (f + n) * (1 + k * k), 1.0f);
            const float radius = 0.5f * std::sqrt((f - n) * (f - n)
                + 2.0f * (f * f + n * n) * k * k
                + (f + n) * (f + n) * k * k * k * k);
            return glm::vec4(center, radius);
        }
    }

    void AbstractCamera::updateProjectionMatrix()
    {
        m_P = invertProjectionY * reverseZPerspective(m_fovY, m_aspectRatio, m_zNear, m_zFar);
    }
}