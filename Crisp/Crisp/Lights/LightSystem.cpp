#include <Crisp/Lights/LightSystem.hpp>

#include <algorithm>
#include <random>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

namespace crisp {
LightSystem::LightSystem(
    Renderer* renderer, const DirectionalLight& dirLight, const uint32_t shadowMapSize, const uint32_t cascadeCount)
    : m_renderer(renderer)
    , m_directionalLight(dirLight)
    , m_directionalLightBuffer(createUniformRingBuffer(&renderer->getDevice(), sizeof(LightDescriptor)))
    , m_lightDepthRange(glm::vec2(0.0f, 100.0f))
    , m_shadowMapSize(shadowMapSize) {
    if (cascadeCount > 0) {
        m_cascadedShadowMapping.configure(&renderer->getDevice(), m_directionalLight, cascadeCount);
    }
}

void LightSystem::update(const Camera& camera, const uint32_t regionIndex) {
    const auto cameraDepthRange = camera.getViewDepthRange();
    const float zNear = std::max(cameraDepthRange.x, m_lightDepthRange.x);
    const float zFar = std::min(cameraDepthRange.y, m_lightDepthRange.y);
    m_cascadedShadowMapping.updateSplitIntervals(zNear, zFar);
    m_cascadedShadowMapping.updateTransforms(camera, m_shadowMapSize, regionIndex);

    if (m_pointLightBuffer) {
        std::vector<LightDescriptor> lightDescriptors;
        lightDescriptors.reserve(m_pointLights.size());
        for (const auto& pl : m_pointLights) {
            lightDescriptors.push_back(pl.createDescriptorData());
        }

        m_pointLightBuffer->updateStagingBufferFromStdVec(lightDescriptors, regionIndex);
    }
}

void LightSystem::setDirectionalLight(const DirectionalLight& dirLight) {
    m_directionalLight = dirLight;
    m_directionalLightBuffer->updateStagingBufferFromStruct(
        m_directionalLight.createDescriptor(), m_renderer->getCurrentVirtualFrameIndex());
    m_cascadedShadowMapping.updateLight(m_directionalLight);
}

void LightSystem::setSplitLambda(const float splitLambda) {
    m_cascadedShadowMapping.splitLambda = splitLambda;
}

void LightSystem::setCascadeBlendFraction(const float blendFraction) {
    m_cascadedShadowMapping.splitBlendFraction = blendFraction;
}

void LightSystem::setCasterDepthExtrusion(const float extrusion) {
    m_cascadedShadowMapping.casterDepthExtrusion = extrusion;
}

void LightSystem::setVisualizeCascades(const bool enabled) {
    m_cascadedShadowMapping.visualizeCascades = enabled;
}

VulkanRingBuffer* LightSystem::getDirectionalLightBuffer() const {
    return m_directionalLightBuffer.get();
}

VulkanRingBuffer* LightSystem::getCascadedDirectionalLightBuffer() const {
    return m_cascadedShadowMapping.cascadedLightBuffer.get();
}

VkDescriptorBufferInfo LightSystem::getCascadedDirectionalLightBufferInfo(uint32_t cascadeIndex) const {
    return m_cascadedShadowMapping.cascadedLightBuffer->getDescriptorInfo(
        cascadeIndex * sizeof(LightDescriptor), sizeof(LightDescriptor));
}

std::array<glm::vec3, Camera::kFrustumPointCount> LightSystem::getCascadeFrustumPoints(uint32_t cascadeIndex) const {
    return m_cascadedShadowMapping.getFrustumPoints(cascadeIndex);
}

float LightSystem::getCascadeSplitLo(uint32_t cascadeIndex) const {
    return m_cascadedShadowMapping.cascades.at(cascadeIndex).zNear;
}

float LightSystem::getCascadeSplitHi(uint32_t cascadeIndex) const {
    return m_cascadedShadowMapping.cascades.at(cascadeIndex).zFar;
}

bool LightSystem::isCascadeCasterVisible(const uint32_t cascadeIndex, const BoundingBox3& worldBounds) const {
    return m_cascadedShadowMapping.isCasterVisible(cascadeIndex, worldBounds);
}

void LightSystem::createPointLightBuffer(std::vector<PointLight>&& pointLights) {
    m_pointLights = std::move(pointLights);

    std::vector<LightDescriptor> lightDescriptors{};
    lightDescriptors.reserve(m_pointLights.size());
    for (const auto& light : m_pointLights) {
        lightDescriptors.emplace_back(light.createDescriptorData());
    }

    m_pointLightBuffer = createStorageRingBuffer(
        &m_renderer->getDevice(), m_pointLights.size() * sizeof(LightDescriptor), lightDescriptors.data());
}

void LightSystem::createTileGridBuffers(const CameraParameters& cameraParams) {
    m_lightClustering.configure(m_renderer, cameraParams);
}

VulkanRingBuffer* LightSystem::getPointLightBuffer() const {
    return m_pointLightBuffer.get();
}

VulkanRingBuffer* LightSystem::getLightIndexBuffer() const {
    return m_lightClustering.m_lightIndexListBuffer.get();
}

const VulkanImageView& LightSystem::getTileGridView() const {
    return *m_lightClustering.m_lightGridView;
}

void LightSystem::setEnvironmentMap(ImageBasedLightingData&& iblData, const std::string& name) {
    if (!m_environmentLight) {
        m_environmentLight = std::make_unique<EnvironmentLight>(*m_renderer, std::move(iblData));
    } else {
        // Update the map, but make sure that all rendering tha touched it so far has finished.
        m_renderer->finish();
        m_environmentLight->update(*m_renderer, iblData);
    }

    m_environmentLight->setName(name);
}

std::vector<PointLight> createRandomPointLights(const uint32_t count) {
    constexpr glm::vec3 kDomainExtent{32.0f * 5.0f, 15.0f * 5.0f, 15.0f * 5.0f};

    std::vector<PointLight> pointLights{};
    pointLights.reserve(count);

    std::default_random_engine eng{};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t i = 0; i < count; ++i) {
        const glm::vec3 spectrum = 5.0f * dist(eng) * glm::vec3(dist(eng), dist(eng), dist(eng));
        const glm::vec3 position = glm::vec3(dist(eng) - 0.5f, dist(eng), dist(eng) - 0.5f) * kDomainExtent;
        pointLights.emplace_back(spectrum, position, glm::vec3(0.0f, 1.0f, 0.0f)).calculateRadius();
    }

    return pointLights;
}

} // namespace crisp
