#pragma once

#include <memory>

#include "Scene.hpp"

#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class Application;
    class CameraController;

    class FluidSimulation;

    class SceneRenderPass;
    class FullScreenQuadPipeline;
    class TextureView;
    class UniformBuffer;
    class VulkanDevice;
    class VulkanRenderer;
    class VulkanSampler;

    class FluidSimulationScene : public Scene
    {
    public:
        FluidSimulationScene(VulkanRenderer* renderer, Application* app);
        ~FluidSimulationScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

    private:
        void initRenderTargetResources();

        VulkanRenderer*  m_renderer;
        VulkanDevice*    m_device;
        Application*     m_app;

        std::unique_ptr<CameraController> m_cameraController;

        std::unique_ptr<FluidSimulation> m_fluidSimulation;

        std::unique_ptr<SceneRenderPass> m_scenePass;
        std::unique_ptr<UniformBuffer> m_cameraBuffer;
        
        std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<TextureView> m_sceneImageView;
        DescriptorSetGroup m_sceneDescSetGroup;
    };
}