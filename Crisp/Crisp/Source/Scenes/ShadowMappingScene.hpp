#pragma once

#include <vector>
#include <memory>
#include <array>

#include "Renderer/Renderer.hpp"
#include "Geometry/TransformPack.hpp"
#include "Renderer/DescriptorSetGroup.hpp"
#include "Scene.hpp"

namespace crisp
{
    struct GaussianBlur
    {
        glm::vec2 texelSize;
        float sigma;
        int radius;
    };

    class Application;
    class CameraController;

    class CascadedShadowMapper;

    class ShadowPass;
    class ShadowMapPipeline;

    class BlurPass;
    class BlurPipeline;

    class VarianceShadowMapPass;
    class VarianceShadowMapPipeline;

    class NormalMapPipeline;

    class SceneRenderPass;
    class BlinnPhongPipeline;
    class ColorAndShadowPipeline;
    class FullScreenQuadPipeline;
    class VulkanImageView;
    class UniformBuffer;
    class VulkanDevice;
    class Renderer;
    class VulkanSampler;
    class MeshGeometry;
    class BoxVisualizer;
    class Skybox;

    class ShadowMappingScene : public Scene
    {
    public:
        ShadowMappingScene(Renderer* renderer, Application* app);
        ~ShadowMappingScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

        void setBlurRadius(int radius);

    private:
        void initRenderTargetResources();

        Renderer*  m_renderer;
        VulkanDevice*    m_device;
        Application*     m_app;
        std::unique_ptr<CameraController> m_cameraController;

        std::unique_ptr<UniformBuffer> m_cameraBuffer;

        uint32_t m_renderMode;

        std::vector<TransformPack> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<CascadedShadowMapper> m_shadowMapper;

        std::unique_ptr<MeshGeometry> m_sphereGeometry;
        std::unique_ptr<MeshGeometry> m_planeGeometry;
        std::unique_ptr<MeshGeometry> m_cwPlaneGeometry;
        std::unique_ptr<MeshGeometry> m_bunnyGeometry;
        std::unique_ptr<MeshGeometry> m_cubeGeometry;

        std::unique_ptr<BoxVisualizer> m_boxVisualizer;
        std::unique_ptr<Skybox> m_skybox;

        std::unique_ptr<ShadowPass> m_shadowPass;
        std::unique_ptr<ShadowMapPipeline> m_shadowMapPipeline;
        DescriptorSetGroup m_shadowMapDescGroup;

        std::vector<glm::mat4> m_lvpTransforms;
        std::unique_ptr<UniformBuffer> m_lvpBuffer;

        std::vector<glm::mat4> m_vsmLightTransforms;
        std::unique_ptr<UniformBuffer> m_vsmLightBuffer;

        std::unique_ptr<SceneRenderPass> m_scenePass;
        std::unique_ptr<BlinnPhongPipeline> m_blinnPhongPipeline;
        std::unique_ptr<ColorAndShadowPipeline> m_colorAndShadowPipeline;
        std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_colorAndShadowDescGroups;
        std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_blinnPhongDescGroups;

        std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        DescriptorSetGroup m_sceneDescSetGroup;
        std::unique_ptr<VulkanImageView> m_sceneImageView;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<VulkanSampler> m_nearestNeighborSampler;
        std::unique_ptr<VulkanSampler> m_linearRepeatSampler;

        std::unique_ptr<BlurPass> m_blurPass;
        std::unique_ptr<BlurPipeline> m_blurPipeline;
        std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_blurDescGroups;

        std::unique_ptr<BlurPass> m_vertBlurPass;
        std::unique_ptr<BlurPipeline> m_vertBlurPipeline;
        std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_vertBlurDescGroups;

        std::unique_ptr<VarianceShadowMapPass> m_vsmPass;
        std::unique_ptr<VarianceShadowMapPipeline> m_vsmPipeline;
        DescriptorSetGroup m_vsmDescSetGroup;

        std::unique_ptr<NormalMapPipeline> m_normalMapPipeline;
        std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_normalMapDescGroups;

        std::unique_ptr<Texture>       m_normalMap;
        std::unique_ptr<VulkanImageView>   m_normalMapView;

        GaussianBlur m_blurParameters;
    };
}