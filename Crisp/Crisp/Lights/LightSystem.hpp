#pragma once

#include <memory>
#include <vector>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Lights/CascadedShadowMapping.hpp>
#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Lights/EnvironmentLight.hpp>
#include <Crisp/Lights/LightClustering.hpp>
#include <Crisp/Lights/LightDescriptor.hpp>
#include <Crisp/Lights/PointLight.hpp>

namespace crisp {
class Renderer;
class RenderGraph;
class VulkanImage;
class VulkanImageView;

class LightSystem {
public:
    LightSystem(Renderer* renderer, const DirectionalLight& dirLight, uint32_t shadowMapSize, uint32_t cascadeCount);

    void update(const Camera& camera, float dt, uint32_t regionIndex);

    void setDirectionalLight(const DirectionalLight& dirLight);

    const DirectionalLight& getDirectionalLight() const {
        return m_directionalLight;
    }

    // Cascaded shadow mapping for directional light.
    std::array<glm::vec3, Camera::kFrustumPointCount> getCascadeFrustumPoints(uint32_t cascadeIndex) const;
    void setSplitLambda(float splitLambda);
    float getCascadeSplitLo(uint32_t cascadeIndex) const;
    float getCascadeSplitHi(uint32_t cascadeIndex) const;

    VulkanRingBuffer* getDirectionalLightBuffer() const;
    VulkanRingBuffer* getCascadedDirectionalLightBuffer() const;
    VkDescriptorBufferInfo getCascadedDirectionalLightBufferInfo(uint32_t cascadeIndex) const;

    void createPointLightBuffer(std::vector<PointLight>&& pointLights);
    void createTileGridBuffers(const CameraParameters& cameraParams);
    void addLightClusteringPass(RenderGraph& renderGraph, const VulkanRingBuffer& cameraBuffer);

    VulkanRingBuffer* getPointLightBuffer() const;
    VulkanRingBuffer* getLightIndexBuffer() const;
    const VulkanImageView& getTileGridView() const;

    EnvironmentLight* getEnvironmentLight() const {
        return m_environmentLight.get();
    }

    void setEnvironmentMap(ImageBasedLightingData&& iblData, const std::string& name);

private:
    Renderer* m_renderer;

    DirectionalLight m_directionalLight;
    std::unique_ptr<VulkanRingBuffer> m_directionalLightBuffer;
    CascadedShadowMapping m_cascadedShadowMapping;

    uint32_t m_shadowMapSize;

    std::vector<PointLight> m_pointLights;
    std::unique_ptr<VulkanRingBuffer> m_pointLightBuffer;
    LightClustering m_lightClustering;

    std::unique_ptr<EnvironmentLight> m_environmentLight;
};

std::vector<PointLight> createRandomPointLights(uint32_t count);

} // namespace crisp
