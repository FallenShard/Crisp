#include "DirectionalLight.hpp"

namespace crisp
{
    DirectionalLight::DirectionalLight(const glm::vec3& direction, const glm::vec3& radiance, const glm::vec3& minCorner, const glm::vec3& maxCorner)
        : m_direction(direction)
        , m_radiance(radiance)
    {
        auto up    = glm::vec3(0.0f, 1.0f, 0.0f);
        auto right = glm::normalize(glm::cross(m_direction, up));
        up         = glm::normalize(glm::cross(right, m_direction));

        auto position = glm::vec3(0.0f, 0.0f, 0.0f);
        m_view     = glm::lookAt(position, position + m_direction, up);

        m_projection = glm::ortho(minCorner.x, maxCorner.x, minCorner.y, maxCorner.y, minCorner.z, maxCorner.z);
    }

    void DirectionalLight::setDirection(glm::vec3 dir)
    {
        m_direction = dir;
    }

    const glm::mat4& DirectionalLight::getViewMatrix() const
    {
        return m_view;
    }

    const glm::mat4& DirectionalLight::getProjectionMatrix() const
    {
        return m_projection;
    }

    void DirectionalLight::calculateProjection(float split, const std::array<glm::vec3, 8>& worldFrustumPoints)
    {
        glm::vec3 minCorner = worldFrustumPoints[0];
        glm::vec3 maxCorner = worldFrustumPoints[0];
        glm::vec3 center = worldFrustumPoints[0];
        for (int i = 1; i < worldFrustumPoints.size(); ++i)
        {
            auto lightViewPt = m_view * glm::vec4(worldFrustumPoints[i], 1.0f);

            minCorner = glm::min(minCorner, glm::vec3(lightViewPt));
            maxCorner = glm::max(maxCorner, glm::vec3(lightViewPt));
            center += worldFrustumPoints[i];
        }
        center /= 8.0f;

        //center.x = std::round(1.0f / 1024) * 1024;
        //center.y = std::round(1.0f / 1024) * 1024;
        //center.z = std::round(1.0f / 1024) * 1024;


        float radius = 0.0f;
        for (uint32_t i = 0; i < 8; i++) {
            float distance = glm::length(worldFrustumPoints[i] - center);
            radius = glm::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 lengths(maxCorner - minCorner);
        auto cubeCenter = (minCorner + maxCorner) / 2.0f;
        auto squareSize = std::max(lengths.x, lengths.y) * 2.0f;

        //lengths.z *= 10.0f;

        glm::vec3 maxExtents = glm::vec3(radius);
        glm::vec3 minExtents = -maxExtents;

        glm::mat4 lightViewMatrix = glm::lookAt(center - m_direction * -minExtents.z, center, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
        m_view = lightViewMatrix;
        m_projection = lightOrthoMatrix;
        //m_view = glm::lookAt(cubeCenter - m_direction * -minCorner.z, cubeCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        //m_projection = glm::ortho(
        //    cubeCenter.x - (squareSize / 2.0f), cubeCenter.x + (squareSize / 2.0f),
        //    cubeCenter.y - (squareSize / 2.0f), cubeCenter.y + (squareSize / 2.0f),
        //    -cubeCenter.z - lengths.z * 14.0f / 2.0f, -cubeCenter.z + lengths.z / 2.0f);
    }

    LightDescriptorData DirectionalLight::createDescriptorData() const
    {
        LightDescriptorData data;
        data.VP       = m_projection * m_view;
        data.V        = m_view;
        data.P        = m_projection;
        data.position = glm::vec4(m_direction, 0.0f);
        data.spectrum = m_radiance;
        return data;
    }
}