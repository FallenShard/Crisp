#pragma once

#include <memory>
#include <unordered_map>

#include "Scene.hpp"

#include "Geometry/TransformPack.hpp"
#include "Renderer/DescriptorSetGroup.hpp"

namespace crisp
{
    class Application;
    class CameraController;

    class FluidSimulation;

    class SceneRenderPass;
    class VulkanPipeline;
    class VulkanImageView;
    class UniformBuffer;
    class VulkanDevice;
    class Renderer;
    class VulkanSampler;
    class RenderGraph;

    class FluidSimulationScene : public AbstractScene
    {
    public:
        FluidSimulationScene(Renderer* renderer, Application* app);
        ~FluidSimulationScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

    private:
        Renderer*     m_renderer;
        Application*  m_app;

        std::unique_ptr<CameraController> m_cameraController;

        std::unique_ptr<FluidSimulation> m_fluidSimulation;

        std::unique_ptr<VulkanPipeline> m_pointSpritePipeline;
        DescriptorSetGroup m_pointSpriteDescGroup;

        TransformPack m_transforms;
        std::unique_ptr<UniformBuffer> m_transformsBuffer;

        struct ParticleParams
        {
            float radius;
            float screenSpaceScale;
        };
        ParticleParams m_particleParams;

        std::unordered_map<std::string, std::unique_ptr<UniformBuffer>> m_uniformBuffers;
        std::unique_ptr<RenderGraph> m_renderGraph;
    };
}