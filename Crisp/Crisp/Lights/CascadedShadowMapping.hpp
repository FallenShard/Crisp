#pragma once

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>

#include <memory>

namespace crisp
{
struct CascadedShadowMapping
{
    // Balances logarithmic and linear splitting of the cascade depths.
    float splitLambda{0.5f};

    struct Cascade
    {
        float zNear;
        float zFar;

        DirectionalLight light;
        std::unique_ptr<UniformBuffer> buffer;
    };

    std::vector<Cascade> cascades;
    std::unique_ptr<UniformBuffer> cascadedLightBuffer;

    void setCascadeCount(Renderer* renderer, const DirectionalLight& light, uint32_t cascadeCount);
    void updateSplitIntervals(float zNear, float zFar);
    void updateTransforms(const Camera& viewCamera, const uint32_t shadowMapSize);

    std::array<glm::vec3, Camera::FrustumPointCount> getFrustumPoints(uint32_t cascadeIndex) const;
};

} // namespace crisp
