#include "LightSystem.hpp"

#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>

#include <Crisp/Camera/Camera.hpp>

#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <random>

namespace crisp
{
LightSystem::LightSystem(
    Renderer* renderer, const DirectionalLight& dirLight, const uint32_t shadowMapSize, const uint32_t cascadeCount)
    : m_renderer(renderer)
    , m_directionalLight(dirLight)
    , m_directionalLightBuffer(
          std::make_unique<UniformBuffer>(renderer, sizeof(LightDescriptor), BufferUpdatePolicy::PerFrame))
    , m_shadowMapSize(shadowMapSize)
{
    if (cascadeCount > 0)
    {
        m_cascadedShadowMapping.setCascadeCount(renderer, m_directionalLight, cascadeCount);
    }
}

void LightSystem::update(const Camera& camera, float /*dt*/)
{
    // const auto [zNear, zFar] = camera.getViewDepthRange();
    const auto [zNear, zFar] = glm::vec2(1.0f, 100.0f);
    m_cascadedShadowMapping.updateSplitIntervals(zNear, zFar);
    m_cascadedShadowMapping.updateTransforms(camera, m_shadowMapSize);

    if (m_pointLightBuffer)
    {
        std::vector<LightDescriptor> lightDescriptors;
        lightDescriptors.reserve(m_pointLights.size());
        for (const auto& pl : m_pointLights)
            lightDescriptors.push_back(pl.createDescriptorData());

        m_pointLightBuffer->updateStagingBuffer(
            lightDescriptors.data(), lightDescriptors.size() * sizeof(LightDescriptor));
    }
}

void LightSystem::setDirectionalLight(const DirectionalLight& dirLight)
{
    m_directionalLight = dirLight;
    m_directionalLightBuffer->updateStagingBuffer(m_directionalLight.createDescriptor());
}

void LightSystem::setSplitLambda(const float splitLambda)
{
    m_cascadedShadowMapping.splitLambda = splitLambda;
}

UniformBuffer* crisp::LightSystem::getDirectionalLightBuffer() const
{
    return m_directionalLightBuffer.get();
}

UniformBuffer* LightSystem::getCascadedDirectionalLightBuffer() const
{
    return m_cascadedShadowMapping.cascadedLightBuffer.get();
}

UniformBuffer* LightSystem::getCascadedDirectionalLightBuffer(uint32_t index) const
{
    return m_cascadedShadowMapping.cascades.at(index).buffer.get();
}

std::array<glm::vec3, Camera::kFrustumPointCount> LightSystem::getCascadeFrustumPoints(uint32_t cascadeIndex) const
{
    return m_cascadedShadowMapping.getFrustumPoints(cascadeIndex);
}

float LightSystem::getCascadeSplitLo(uint32_t cascadeIndex) const
{
    return m_cascadedShadowMapping.cascades.at(cascadeIndex).zNear;
}

float LightSystem::getCascadeSplitHi(uint32_t cascadeIndex) const
{
    return m_cascadedShadowMapping.cascades.at(cascadeIndex).zFar;
}

void LightSystem::createPointLightBuffer(std::vector<PointLight>&& pointLights)
{
    m_pointLights = std::move(pointLights);

    std::vector<LightDescriptor> lightDescriptors{};
    lightDescriptors.reserve(m_pointLights.size());
    for (const auto& light : m_pointLights)
    {
        lightDescriptors.emplace_back(light.createDescriptorData());
    }

    m_pointLightBuffer = std::make_unique<UniformBuffer>(
        m_renderer,
        m_pointLights.size() * sizeof(LightDescriptor),
        true,
        BufferUpdatePolicy::PerFrame,
        lightDescriptors.data());
}

void LightSystem::createTileGridBuffers(const CameraParameters& cameraParams)
{
    m_lightClustering.configure(m_renderer, cameraParams, static_cast<uint32_t>(m_pointLights.size()));
}

void LightSystem::addLightClusteringPass(RenderGraph& renderGraph, const UniformBuffer& cameraBuffer)
{
    addToRenderGraph(m_renderer, renderGraph, m_lightClustering, cameraBuffer, *m_pointLightBuffer);
}

UniformBuffer* LightSystem::getPointLightBuffer() const
{
    return m_pointLightBuffer.get();
}

UniformBuffer* LightSystem::getLightIndexBuffer() const
{
    return m_lightClustering.m_lightIndexListBuffer.get();
}

const std::vector<std::unique_ptr<VulkanImageView>>& LightSystem::getTileGridViews() const
{
    return m_lightClustering.m_lightGridViews;
}

std::vector<PointLight> createRandomPointLights(const uint32_t count)
{
    constexpr float kWidth = 32.0f * 5.0f;
    constexpr float kDepth = 15.0f * 5.0f;
    constexpr float kHeight = 15.0f * 5.0f;
    constexpr glm::vec3 kDomainExtent{kWidth, kHeight, kDepth};
    const int32_t gridX = 16;
    const int32_t gridZ = 8;
    const int32_t gridY = count / gridX / gridZ;

    std::vector<PointLight> pointLights{};
    pointLights.reserve(count);

    std::default_random_engine eng{};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int32_t i = 0; i < gridX; ++i)
    {
        // const float normI = static_cast<float>(i) / static_cast<float>(gridX - 1);
        for (int32_t j = 0; j < gridZ; ++j)
        {
            // const float normJ = static_cast<float>(j) / static_cast<float>(gridZ - 1);
            for (int32_t k = 0; k < gridY; ++k)
            {
                // const float normK = static_cast<float>(k) / static_cast<float>(gridY - 1);
                const glm::vec3 spectrum = 5.0f * dist(eng) * glm::vec3(dist(eng), dist(eng), dist(eng));
                /*const glm::vec3 position =
                    glm::vec3((normJ - 0.5f) * width, 1.0f + normK * height, (normI - 0.5f) * depth);*/

                const glm::vec3 stratifiedPos =
                    glm::vec3(dist(eng) - 0.5f, dist(eng), dist(eng) - 0.5f) * kDomainExtent;
                pointLights.emplace_back(spectrum, stratifiedPos, glm::vec3(0.0f, 1.0f, 0.0f)).calculateRadius();
            }
        }
    }

    return pointLights;
}
} // namespace crisp
