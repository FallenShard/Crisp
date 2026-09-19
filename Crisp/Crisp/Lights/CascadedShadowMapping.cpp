#include <Crisp/Lights/CascadedShadowMapping.hpp>

#include <cmath>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
bool intersectsClipVolume(const glm::mat4& viewProjection, const BoundingBox3& bounds) {
    if (!bounds.isValid()) {
        return true;
    }

    constexpr uint32_t kAllClipPlanes{0x3fu};
    uint32_t commonOutsidePlanes{kAllClipPlanes};
    for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
        const glm::vec4 clip = viewProjection * glm::vec4(bounds.getCorner(cornerIndex), 1.0f);
        uint32_t outsidePlanes{0};
        outsidePlanes |= clip.x < -clip.w ? 1u << 0 : 0u;
        outsidePlanes |= clip.x > +clip.w ? 1u << 1 : 0u;
        outsidePlanes |= clip.y < -clip.w ? 1u << 2 : 0u;
        outsidePlanes |= clip.y > +clip.w ? 1u << 3 : 0u;
        outsidePlanes |= clip.z < 0.0f ? 1u << 4 : 0u;
        outsidePlanes |= clip.z > clip.w ? 1u << 5 : 0u;
        commonOutsidePlanes &= outsidePlanes;
    }
    return commonOutsidePlanes == 0;
}

void CascadedShadowMapping::configure(
    const gsl::not_null<VulkanDevice*> device, const DirectionalLight& light, const uint32_t cascadeCount) {
    cascades.resize(cascadeCount);
    for (auto& cascade : cascades) {
        cascade.light = light;
    }
    cascadedLightBuffer = createUniformRingBuffer(device, cascadeCount * sizeof(LightDescriptor));
}

void CascadedShadowMapping::updateLight(const DirectionalLight& light) {
    for (auto& cascade : cascades) {
        cascade.light = light;
    }
}

void CascadedShadowMapping::updateSplitIntervals(const float zNear, const float zFar) {
    if (cascades.empty()) {
        return;
    }

    CRISP_CHECK(std::isfinite(zNear) && zNear > 0.0f, "The cascade near plane must be finite and positive.");
    CRISP_CHECK(std::isfinite(zFar) && zFar > zNear, "The cascade far plane must be finite and beyond its near plane.");
    CRISP_CHECK(
        std::isfinite(splitLambda) && splitLambda >= 0.0f && splitLambda <= 1.0f,
        "The cascade split lambda must be finite and in [0, 1].");
    CRISP_CHECK(
        std::isfinite(splitBlendFraction) && splitBlendFraction >= 0.0f && splitBlendFraction < 1.0f,
        "The cascade blend fraction must be finite and in [0, 1).");

    const float range = zFar - zNear;
    const float ratio = zFar / zNear;

    cascades.front().zNear = zNear;
    cascades.back().zFar = zFar;
    for (uint32_t i = 0; i < cascades.size() - 1; i++) {
        const float p = static_cast<float>(i + 1) / static_cast<float>(cascades.size());
        const float logSplit = zNear * std::pow(ratio, p);
        const float linSplit = zNear + range * p;
        const float splitPos = splitLambda * (logSplit - linSplit) + linSplit;
        cascades[i].zFar = splitPos;
        cascades[i + 1].zNear = splitPos;
    }

    for (uint32_t i = 0; i < cascades.size(); ++i) {
        auto& cascade = cascades[i];
        cascade.blendStart =
            i + 1 < cascades.size() ? cascade.zFar - splitBlendFraction * (cascade.zFar - cascade.zNear) : cascade.zFar;
    }
}

void CascadedShadowMapping::updateTransforms(
    const Camera& viewCamera, const uint32_t shadowMapSize, const uint32_t regionIndex) {
    for (uint32_t i = 0; i < cascades.size(); ++i) {
        auto& cascade = cascades[i];

        const float fitNear = i == 0 ? cascade.zNear : cascades[i - 1].blendStart;
        const glm::vec4 centerRadius = viewCamera.computeFrustumBoundingSphere(fitNear, cascade.zFar);
        cascade.light.fitProjectionToBoundingSphere(centerRadius, centerRadius.w, shadowMapSize, casterDepthExtrusion);

        auto desc = cascade.light.createDescriptor();
        desc.position.w = visualizeCascades ? 1.0f : 0.0f;
        desc.params = glm::vec4(cascade.zNear, cascade.zFar, cascade.blendStart, cascade.light.getWorldUnitsPerTexel());
        cascadedLightBuffer->updateStagingBuffer(
            {
                .data = &desc,
                .size = sizeof(LightDescriptor),
                .dstOffset = i * sizeof(LightDescriptor),
            },
            regionIndex);
    }
}

glm::mat4 CascadedShadowMapping::getCascadeViewProjection(const uint32_t cascadeIndex) const {
    const auto& light = cascades.at(cascadeIndex).light;
    return light.getProjectionMatrix() * light.getViewMatrix();
}

bool CascadedShadowMapping::isCasterVisible(const uint32_t cascadeIndex, const BoundingBox3& worldBounds) const {
    return intersectsClipVolume(getCascadeViewProjection(cascadeIndex), worldBounds);
}

std::array<glm::vec3, Camera::kFrustumPointCount> CascadedShadowMapping::getFrustumPoints(uint32_t cascadeIndex) const {
    std::array<glm::vec3, 8> frustumPoints = {
        glm::vec3(-1.0f, -1.0f, 0.0f),
        glm::vec3(+1.0f, -1.0f, 0.0f),
        glm::vec3(+1.0f, +1.0f, 0.0f),
        glm::vec3(-1.0f, +1.0f, 0.0f),
        glm::vec3(-1.0f, -1.0f, 1.0f),
        glm::vec3(+1.0f, -1.0f, 1.0f),
        glm::vec3(+1.0f, +1.0f, 1.0f),
        glm::vec3(-1.0f, +1.0f, 1.0f),
    };

    const auto lightToWorld = glm::inverse(cascades.at(cascadeIndex).light.createDescriptor().VP);
    for (auto& p : frustumPoints) {
        p = glm::vec3(lightToWorld * glm::vec4(p, 1.0f));
    }

    return frustumPoints;
}

} // namespace crisp
