#include "DirectionalLight.hpp"

namespace crisp
{
    DirectionalLight::DirectionalLight(const glm::vec3& direction)
        : m_direction(direction)
    {
        auto up    = glm::vec3(0.0f, 1.0f, 0.0f);
        auto right = glm::normalize(glm::cross(m_direction, up));
        up         = glm::normalize(glm::cross(right, m_direction));

        auto position = glm::vec3(0.0f, 0.0f, 0.0f);
        m_view     = glm::lookAt(position, position + m_direction, up);
    }

    const glm::mat4& DirectionalLight::getViewMatrix() const
    {
        return m_view;
    }

    const glm::mat4& DirectionalLight::getProjectionMatrix() const
    {
        return m_projection;
    }

    void DirectionalLight::calculateProjection(const std::array<glm::vec3, 8>& worldFrustumPoints)
    {
        glm::vec3 minCorner(1000.0f);
        glm::vec3 maxCorner(-1000.0f);
        for (auto& pt : worldFrustumPoints)
        {
            auto lightViewPt = m_view * glm::vec4(pt, 1.0f);

            minCorner = glm::min(minCorner, glm::vec3(lightViewPt));
            maxCorner = glm::max(maxCorner, glm::vec3(lightViewPt));
        }

        glm::vec3 lengths(maxCorner - minCorner);
        auto cubeCenter = (minCorner + maxCorner) / 2.0f;
        auto squareSize = std::max(lengths.x, lengths.y);

        m_projection = glm::ortho(
            cubeCenter.x - (squareSize / 2.0f), cubeCenter.x + (squareSize / 2.0f),
            cubeCenter.y - (squareSize / 2.0f), cubeCenter.y + (squareSize / 2.0f),
            -maxCorner.z * 1.3f, -minCorner.z * 1.3f);
    }
}