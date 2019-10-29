#pragma once

#include <CrispCore/Math/Headers.hpp>

#include "LightDescriptor.hpp"

namespace crisp
{
    class PointLight
    {
    public:
        PointLight(const glm::vec3& power, const glm::vec3& position, const glm::vec3& directionHint);

        const glm::mat4& getViewMatrix() const;
        const glm::mat4& getProjectionMatrix() const;

        glm::mat4 createModelMatrix(float scale) const;

        LightDescriptor createDescriptorData() const;

    private:
        glm::vec3 m_power;

        glm::vec4 m_position;
        glm::vec3 m_direction;

        glm::mat4 m_view;
        glm::mat4 m_projection;
    };
}