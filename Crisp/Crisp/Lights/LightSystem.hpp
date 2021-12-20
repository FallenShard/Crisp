#pragma once

#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Lights/LightDescriptor.hpp>
#include <Crisp/Lights/PointLight.hpp>

#include <memory>
#include <vector>

namespace crisp
{
class Renderer;
class RenderGraph;
class VulkanImage;
class VulkanImageView;
class UniformBuffer;
class Camera;
struct CameraParameters;

class LightSystem
{
public:
    LightSystem(Renderer* renderer, uint32_t shadowMapSize);

    void update(const Camera& camera, float dt);

    void setDirectionalLight(const DirectionalLight& dirLight);
    inline const DirectionalLight& getDirectionalLight() const
    {
        return *m_directionalLight;
    }
    void enableCascadedShadowMapping(uint32_t cascadeCount, uint32_t shadowMapSize);
    std::array<glm::vec3, 8> getCascadeFrustumPoints(uint32_t cascadeIndex) const;
    void setSplitLambda(float splitLambda);
    float getCascadeSplitLo(uint32_t cascadeIndex) const;
    float getCascadeSplitHi(uint32_t cascadeIndex) const;

    UniformBuffer* getDirectionalLightBuffer() const;
    UniformBuffer* getCascadedDirectionalLightBuffer() const;
    UniformBuffer* getCascadedDirectionalLightBuffer(uint32_t index) const;

    void createPointLightBuffer(uint32_t pointLightCount);
    void createTileGridBuffers(const CameraParameters& cameraParams);
    void addLightClusteringPass(RenderGraph& renderGraph, const UniformBuffer& cameraBuffer);

    UniformBuffer* getPointLightBuffer() const;
    UniformBuffer* getLightIndexBuffer() const;
    const std::vector<std::unique_ptr<VulkanImageView>>& getTileGridViews() const;

private:
    void updateSplitIntervals(float zNear, float zFar);

    Renderer* m_renderer;

    std::unique_ptr<DirectionalLight> m_directionalLight;
    std::unique_ptr<UniformBuffer> m_directionalLightBuffer;

    uint32_t m_shadowMapSize;

    float m_splitLambda;
    struct Cascade
    {
        float begin;
        float end;

        std::unique_ptr<DirectionalLight> light;
        std::unique_ptr<UniformBuffer> buffer;
    };

    std::vector<Cascade> m_cascades;
    std::unique_ptr<UniformBuffer> m_cascadedDirectionalLightBuffer;

    std::vector<PointLight> m_pointLights;
    std::unique_ptr<UniformBuffer> m_pointLightBuffer;

    glm::ivec2 m_tileSize;
    glm::ivec2 m_tileGridDimensions;
    std::size_t m_tileCount;
    std::unique_ptr<UniformBuffer> m_tilePlaneBuffer;
    std::unique_ptr<UniformBuffer> m_lightIndexCountBuffer;
    std::unique_ptr<UniformBuffer> m_lightIndexListBuffer;
    std::unique_ptr<VulkanImage> m_lightGrid;
    std::vector<std::unique_ptr<VulkanImageView>> m_lightGridViews;

    // many light buffer
    // shadow maps

    // std::vector<ConeLight> m_coneLights;
};
} // namespace crisp
