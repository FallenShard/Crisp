#pragma once

#include <Crisp/Lights/LightDescriptor.hpp>

namespace crisp
{
class PointLight
{
public:
    PointLight() {}
    PointLight(const glm::vec3& power, const glm::vec3& position, const glm::vec3& directionHint);

    const glm::mat4& getViewMatrix() const;
    const glm::mat4& getProjectionMatrix() const;

    glm::mat4 createModelMatrix(float scale) const;

    inline void setPosition(glm::vec4 pos)
    {
        m_position = pos;
    }

    LightDescriptor createDescriptorData() const;

    inline void calculateRadius(float cutoff = 1.0f / 256.0f)
    {
        float maxIllum = std::max(m_power.r, std::max(m_power.g, m_power.b));
        m_radius = std::sqrt(maxIllum / cutoff);
    }

private:
    glm::vec3 m_power;

    glm::vec4 m_position;
    glm::vec3 m_direction;

    glm::mat4 m_view;
    glm::mat4 m_projection;

    float m_radius;
};
} // namespace crisp