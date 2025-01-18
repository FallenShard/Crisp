#pragma once

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
struct CascadedShadowMapping {
    // Balances logarithmic and linear splitting of the cascade depths.
    float splitLambda{0.5f};

    struct Cascade {
        float zNear;
        float zFar;

        DirectionalLight light;
    };

    std::vector<Cascade> cascades;
    std::unique_ptr<VulkanRingBuffer> cascadedLightBuffer;

    void setCascadeCount(Renderer* renderer, const DirectionalLight& light, uint32_t cascadeCount);
    void updateDirectionalLight(const DirectionalLight& light);
    void updateSplitIntervals(float zNear, float zFar);
    void updateTransforms(const Camera& viewCamera, uint32_t shadowMapSize, uint32_t regionIndex);

    std::array<glm::vec3, Camera::kFrustumPointCount> getFrustumPoints(uint32_t cascadeIndex) const;
};

} // namespace crisp
