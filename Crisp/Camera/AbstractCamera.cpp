#include "AbstractCamera.hpp"

namespace crisp
{
    namespace
    {
        glm::mat4 invertProjectionY = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));

        glm::mat4 reverseZPerspective(float fovY, float aspectRatio, float zNear, float zFar)
        {
            glm::mat4 test = glm::perspective(fovY, aspectRatio, zNear, 500.0f);
            float f = 1.0f / std::tan(fovY / 2.0f);
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
            //return glm::perspective(fovY, aspectRatio, zFar, zNear);
            //return glm::perspective(fovY, aspectRatio, 500.0f, zNear);
        }
    }

    AbstractCamera::AbstractCamera()
        : m_recalculateViewMatrix(true)
        , m_aspectRatio(1.0f)
        , m_fov(45.0f)
        , m_zFar(100.0f)
        , m_zNear(1.0f)
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

        updateProjectionMatrix();
    }

    void AbstractCamera::setupProjection(float fovY, float aspectRatio)
    {
        m_fov         = glm::radians(fovY);
        m_aspectRatio = aspectRatio;

        updateProjectionMatrix();
    }

    const glm::mat4& AbstractCamera::getProjectionMatrix() const
    {
        return m_P;
    }

    void AbstractCamera::setFov(float fovY)
    {
        m_fov = glm::radians(fovY);
        updateProjectionMatrix();

        m_recalculateViewMatrix = true;
    }

    float AbstractCamera::getFov() const
    {
        return glm::degrees(m_fov);
    }

    void AbstractCamera::setApectRatio(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
        updateProjectionMatrix();

        m_recalculateViewMatrix = true;
    }

    float AbstractCamera::getAspectRatio() const
    {
        return m_aspectRatio;
    }

    void AbstractCamera::setNearPlaneDistance(float zNear)
    {
        m_zNear = zNear;
        updateProjectionMatrix();

        m_recalculateViewMatrix = true;
    }

    float AbstractCamera::getNearPlaneDistance() const
    {
        return m_zNear;
    }

    void AbstractCamera::setFarPlaneDistance(float zFar)
    {
        m_zFar = zFar;
        updateProjectionMatrix();

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

    std::array<glm::vec4, AbstractCamera::NumFrustumPlanes> AbstractCamera::getFrustumPlanes() const
    {
        return m_frustumPlanes;
    }

    glm::vec4 AbstractCamera::getFrustumPlane(size_t index) const
    {
        return m_frustumPlanes.at(index);
    }

    std::array<glm::vec3, 8> AbstractCamera::getFrustumPoints(float zNear, float zFar) const
    {
        auto tanA = std::tan(m_fov / 2.0f);

        auto halfNearH = zNear * tanA;
        auto halfNearW = halfNearH * m_aspectRatio;

        auto halfFarH = zFar * tanA;
        auto halfFarW = halfFarH * m_aspectRatio;

        std::array<glm::vec3, 8> frustumPoints =
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

        std::array<glm::vec4, 8> ffs =
        {
            glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            glm::vec4(+1.0f, -1.0f, 0.0f, 1.0f),
            glm::vec4(+1.0f, +1.0f, 0.0f, 1.0f),
            glm::vec4(-1.0f, +1.0f, 0.0f, 1.0f),
            glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f),
            glm::vec4(+1.0f, -1.0f, 1.0f, 1.0f),
            glm::vec4(+1.0f, +1.0f, 1.0f, 1.0f),
            glm::vec4(-1.0f, +1.0f, 1.0f, 1.0f)
        };

        //glm::mat4 inverseP = glm::inverse(glm::perspective(m_fov, m_aspectRatio, zNear, zFar));
        //for (auto& ff : ffs)
        //{
        //    ff = inverseP * ff;
        //    ff /= ff.w;
        //}


        //std::array<glm::vec3, 8> frustumPoints =
        //{
        //    glm::vec3(-1.0f, -1.0f, 0.0f),
        //    glm::vec3(+1.0f, -1.0f, 0.0f),
        //    glm::vec3(+1.0f, +1.0f, 0.0f),
        //    glm::vec3(-1.0f, +1.0f, 0.0f),
        //    glm::vec3(-1.0f, -1.0f, 1.0f),
        //    glm::vec3(+1.0f, -1.0f, 1.0f),
        //    glm::vec3(+1.0f, +1.0f, 1.0f),
        //    glm::vec3(-1.0f, +1.0f, 1.0f)
        //};

        //glm::mat4 tempP = invertProjectionY * glm::perspective(m_fov, m_aspectRatio, zNear, zFar);
        auto camToWorld = glm::inverse(m_V);
        for (auto& p : frustumPoints)
            p = glm::vec3(camToWorld * glm::vec4(p, 1.0f));

        return frustumPoints;
    }

    std::array<glm::vec3, 8> AbstractCamera::getFrustumPoints() const
    {
        return getFrustumPoints(m_zNear, m_zFar);
    }

    glm::vec4 AbstractCamera::calculateBoundingSphere(float n, float f) const
    {
        float k = std::sqrt(1.0f + m_aspectRatio * m_aspectRatio) * std::tan(m_fov / 2.0f);
        if (k * k >= (f - n) / (f + n))
        {
            glm::vec3 center(0.0f, 0.0f, -f);
            center = glm::inverse(m_V) * glm::vec4(center, 1.0f);
            return glm::vec4(center, f * k);
        }
        else
        {
            glm::vec3 center(0.0f, 0.0f, -0.5f * (f + n) * (1 + k * k));
            center = glm::inverse(m_V) * glm::vec4(center, 1.0f);
            float radius = 0.5f * std::sqrt((f - n) * (f - n)
                + 2.0f * (f * f + n * n) * k * k
                + (f + n) * (f + n) * k * k * k * k);
            return glm::vec4(center, radius);
        }
    }

    void AbstractCamera::updateProjectionMatrix()
    {
        m_P = invertProjectionY * glm::perspective(m_fov, m_aspectRatio, m_zNear, m_zFar);
        m_P = invertProjectionY * reverseZPerspective(m_fov, m_aspectRatio, m_zNear, m_zFar);
    }
}