#pragma once

#include <cstdint>

#include <Crisp/Lights/LightDescriptor.hpp>

namespace crisp {
class DirectionalLight {
public:
    DirectionalLight();
    DirectionalLight(
        const glm::vec3& direction, const glm::vec3& radiance, const glm::vec3& extentMin, const glm::vec3& extentMax);

    void setDirection(glm::vec3 direction);
    void setRadiance(const glm::vec3& radiance);
    void fitProjectionToBoundingSphere(
        const glm::vec3& center, float radius, uint32_t shadowMapSize, float casterDepthExtrusion = 0.0f);

    LightDescriptor createDescriptor() const;

    const glm::vec3& getDirection() const;
    const glm::vec3& getRadiance() const;
    const glm::mat4& getViewMatrix() const;
    const glm::mat4& getProjectionMatrix() const;
    float getWorldUnitsPerTexel() const;

private:
    glm::vec3 m_direction;
    glm::vec3 m_radiance;
    glm::vec3 m_up;

    glm::mat4 m_view;
    glm::mat4 m_projection;
    float m_worldUnitsPerTexel{0.0f};
};
} // namespace crisp
