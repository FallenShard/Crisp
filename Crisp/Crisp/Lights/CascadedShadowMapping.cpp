#include <Crisp/Lights/CascadedShadowMapping.hpp>

namespace crisp {
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

    const float range = zFar - zNear;
    const float ratio = zFar / zNear;

    cascades.front().zNear = zNear;
    cascades.back().zFar = zFar;
    for (uint32_t i = 0; i < cascades.size() - 1; i++) {
        const float p = static_cast<float>(i + 1) / static_cast<float>(cascades.size());
        const float logSplit = zNear * std::pow(ratio, p);
        const float linSplit = zNear + range * p;
        const float splitPos = splitLambda * (logSplit - linSplit) + linSplit;
        cascades[i].zFar = splitPos - zNear;
        cascades[i + 1].zNear = splitPos - zNear;
    }
}

void CascadedShadowMapping::updateTransforms(
    const Camera& viewCamera, const uint32_t shadowMapSize, const uint32_t regionIndex) {
    for (uint32_t i = 0; i < cascades.size(); ++i) {
        auto& cascade = cascades[i];

        glm::vec4 centerRadius = viewCamera.computeFrustumBoundingSphere(cascade.zNear, cascade.zFar);
        cascade.light.fitProjectionToFrustum(
            viewCamera.computeFrustumPoints(cascade.zNear, cascade.zFar), centerRadius, centerRadius.w, shadowMapSize);

        const auto desc = cascade.light.createDescriptor();
        cascadedLightBuffer->updateStagingBuffer(
            {
                .data = &desc,
                .size = sizeof(LightDescriptor),
                .dstOffset = i * sizeof(LightDescriptor),
            },
            regionIndex);
    }
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
