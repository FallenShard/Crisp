#pragma once

#include <memory>

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

    class FluidSimulationScene : public Scene
    {
    public:
        FluidSimulationScene(Renderer* renderer, Application* app);
        ~FluidSimulationScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

    private:
        Renderer*     m_renderer;
        VulkanDevice* m_device;
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
        std::unique_ptr<UniformBuffer> m_paramsBuffer;

        std::unique_ptr<SceneRenderPass> m_scenePass;
        std::unique_ptr<UniformBuffer> m_cameraBuffer;
    };
}