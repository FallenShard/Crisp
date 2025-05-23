#pragma once

#include <array>

#include <Crisp/Lights/LightDescriptor.hpp>

namespace crisp {
class DirectionalLight {
public:
    DirectionalLight() = default;
    DirectionalLight(
        const glm::vec3& direction, const glm::vec3& radiance, const glm::vec3& extentMin, const glm::vec3& extentMax);

    void setDirection(glm::vec3 direction);
    void fitProjectionToFrustum(const std::array<glm::vec3, 8>& worldFrustumPoints);
    void fitProjectionToFrustum(
        const std::array<glm::vec3, 8>& worldFrustumPoints,
        const glm::vec3& center,
        float radius,
        uint32_t shadowMapSize);

    LightDescriptor createDescriptor() const;

    const glm::vec3& getDirection() const;
    const glm::mat4& getViewMatrix() const;
    const glm::mat4& getProjectionMatrix() const;

private:
    glm::vec3 m_direction;
    glm::vec3 m_radiance;

    glm::mat4 m_view;
    glm::mat4 m_projection;
};
} // namespace crisp