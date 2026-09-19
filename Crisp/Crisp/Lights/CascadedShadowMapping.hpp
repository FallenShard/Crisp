#pragma once

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Math/BoundingBox.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
struct CascadedShadowMapping {
    // Balances logarithmic and linear splitting of the cascade depths.
    float splitLambda{0.5f};
    // Fraction of each cascade interval blended into the next cascade. Zero disables blending.
    float splitBlendFraction{0.1f};
    // Extends the cascade toward the light to retain off-slice shadow casters.
    float casterDepthExtrusion{50.0f};
    bool visualizeCascades{false};

    struct Cascade {
        float zNear{};
        float zFar{};
        float blendStart{};

        DirectionalLight light;
    };

    std::vector<Cascade> cascades;

    // Contains 1 light descriptor for each cascade.
    std::unique_ptr<VulkanRingBuffer> cascadedLightBuffer;

    void configure(gsl::not_null<VulkanDevice*> device, const DirectionalLight& light, uint32_t cascadeCount);
    void updateLight(const DirectionalLight& light);
    void updateSplitIntervals(float zNear, float zFar);
    void updateTransforms(const Camera& viewCamera, uint32_t shadowMapSize, uint32_t regionIndex);

    std::array<glm::vec3, Camera::kFrustumPointCount> getFrustumPoints(uint32_t cascadeIndex) const;
    bool isCasterVisible(uint32_t cascadeIndex, const BoundingBox3& worldBounds) const;

    // Hoist this out of per-draw culling loops; it costs a matrix multiply.
    glm::mat4 getCascadeViewProjection(uint32_t cascadeIndex) const;
};

bool intersectsClipVolume(const glm::mat4& viewProjection, const BoundingBox3& bounds);

} // namespace crisp
