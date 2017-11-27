#pragma once

#include <vector>
#include <memory>
#include <array>

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/Transforms.hpp"
#include "Renderer/DescriptorSetGroup.hpp"
#include "Scene.hpp"

namespace crisp
{
    class Application;
    class InputDispatcher;
    class CameraController;

    class CascadedShadowMapper;

    class SceneRenderPass;
    class BlinnPhongPipeline;
    class FullScreenQuadPipeline;
    class TextureView;
    class UniformBuffer;
    class VulkanDevice;
    class VulkanRenderer;
    class VulkanSampler;
    class MeshGeometry;
    class BoxVisualizer;
    class Skybox;

    class ShadowMappingScene : public Scene
    {
    public:
        ShadowMappingScene(VulkanRenderer* renderer, Application* app);
        ~ShadowMappingScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

    private:
        void initRenderTargetResources();

        VulkanRenderer*  m_renderer;
        VulkanDevice*    m_device;
        Application*     m_app;
        std::unique_ptr<CameraController> m_cameraController;

        std::unique_ptr<UniformBuffer> m_cameraBuffer;

        uint32_t m_renderMode;

        std::vector<Transforms> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        std::unique_ptr<CascadedShadowMapper> m_shadowMapper;

        std::unique_ptr<MeshGeometry> m_sphereGeometry;
        std::unique_ptr<MeshGeometry> m_planeGeometry;
        std::unique_ptr<MeshGeometry> m_bunnyGeometry;

        std::unique_ptr<BoxVisualizer> m_boxVisualizer;
        std::unique_ptr<Skybox> m_skybox;

        std::unique_ptr<SceneRenderPass> m_scenePass;
        std::unique_ptr<BlinnPhongPipeline> m_blinnPhongPipeline;
        std::array<DescriptorSetGroup, VulkanRenderer::NumVirtualFrames> m_blinnPhongDescGroups;

        std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        DescriptorSetGroup m_sceneDescSetGroup;
        std::unique_ptr<TextureView> m_sceneImageView;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<VulkanSampler> m_nearestNeighborSampler;
    };
}