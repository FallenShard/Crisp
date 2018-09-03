#pragma once

#include <array>

#include <CrispCore/Math/Headers.hpp>

#include "LightDescriptorData.hpp"

namespace crisp
{
    class DirectionalLight
    {
    public:
        DirectionalLight(const glm::vec3& direction, const glm::vec3& radiance, const glm::vec3& minCorner, const glm::vec3& maxCorner);

        void setDirection(glm::vec3 dir);
        inline glm::vec3 getDirection() const { return m_direction; }

        const glm::mat4& getViewMatrix() const;
        const glm::mat4& getProjectionMatrix() const;

        void calculateProjection(float split, const std::array<glm::vec3, 8>& worldFrustumPoints);

        LightDescriptorData createDescriptorData() const;

    private:
        glm::vec3 m_direction;
        glm::vec3 m_radiance;

        glm::mat4 m_view;
        glm::mat4 m_projection;
    };
}