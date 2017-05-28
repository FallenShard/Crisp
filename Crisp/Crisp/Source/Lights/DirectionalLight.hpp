#pragma once

#include <array>

#include "Math/Headers.hpp"

namespace crisp
{
    class DirectionalLight
    {
    public:
        DirectionalLight(const glm::vec3& direction);

        const glm::mat4& getViewMatrix() const;
        const glm::mat4& getProjectionMatrix() const;

        void calculateProjection(const std::array<glm::vec3, 8>& worldFrustumPoints);

    private:
        glm::vec3 m_direction;

        glm::mat4 m_view;
        glm::mat4 m_projection;
    };
}