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
class UniformBuffer;

class LightSystem {
public:
    LightSystem(Renderer* renderer, const DirectionalLight& dirLight, uint32_t shadowMapSize, uint32_t cascadeCount);

    void update(const Camera& camera, float dt);

    void setDirectionalLight(const DirectionalLight& dirLight);

    inline const DirectionalLight& getDirectionalLight() const {
        return m_directionalLight;
    }

    // Cascaded shadow mapping for directional light.
    std::array<glm::vec3, Camera::kFrustumPointCount> getCascadeFrustumPoints(uint32_t cascadeIndex) const;
    void setSplitLambda(float splitLambda);
    float getCascadeSplitLo(uint32_t cascadeIndex) const;
    float getCascadeSplitHi(uint32_t cascadeIndex) const;

    UniformBuffer* getDirectionalLightBuffer() const;
    UniformBuffer* getCascadedDirectionalLightBuffer() const;
    UniformBuffer* getCascadedDirectionalLightBuffer(uint32_t index) const;

    void createPointLightBuffer(std::vector<PointLight>&& pointLights);
    void createTileGridBuffers(const CameraParameters& cameraParams);
    void addLightClusteringPass(RenderGraph& renderGraph, const UniformBuffer& cameraBuffer);

    UniformBuffer* getPointLightBuffer() const;
    UniformBuffer* getLightIndexBuffer() const;
    const std::vector<std::unique_ptr<VulkanImageView>>& getTileGridViews() const;

    EnvironmentLight* getEnvironmentLight() const {
        return m_environmentLight.get();
    }

    void setEnvironmentMap(ImageBasedLightingData&& iblData, const std::string& name);

private:
    Renderer* m_renderer;

    DirectionalLight m_directionalLight;
    std::unique_ptr<UniformBuffer> m_directionalLightBuffer;
    CascadedShadowMapping m_cascadedShadowMapping;

    uint32_t m_shadowMapSize;

    std::vector<PointLight> m_pointLights;
    std::unique_ptr<UniformBuffer> m_pointLightBuffer;
    LightClustering m_lightClustering;

    // TODO: Implement omnidirectional shadow maps for select point lights.
    // std::vector<ConeLight> m_coneLights;

    std::unique_ptr<EnvironmentLight> m_environmentLight;
};

std::vector<PointLight> createRandomPointLights(uint32_t count);

} // namespace crisp
