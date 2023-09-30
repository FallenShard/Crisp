#pragma once

#include <memory>
#include <unordered_map>

#include "Scene.hpp"

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Geometry/TransformPack.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/RenderTargetCache.hpp>

namespace crisp {
class FluidSimulation;

class SceneRenderPass;
class VulkanPipeline;
class VulkanImageView;
class UniformBuffer;
class VulkanDevice;
class VulkanSampler;

class FluidSimulationScene : public Scene {
public:
    FluidSimulationScene(Renderer* renderer, Window* window);
    ~FluidSimulationScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

private:
    void createGui();

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::unique_ptr<FluidSimulation> m_fluidSimulation;

    std::unique_ptr<VulkanPipeline> m_pointSpritePipeline;
    std::unique_ptr<Material> m_pointSpriteMaterial;

    TransformPack m_transforms;
    std::unique_ptr<UniformBuffer> m_transformsBuffer;

    struct ParticleParams {
        float radius;
        float screenSpaceScale;
    };

    ParticleParams m_particleParams;

    std::unordered_map<std::string, std::unique_ptr<UniformBuffer>> m_uniformBuffers;

    RenderNode m_fluidRenderNode;

    std::unique_ptr<Geometry> m_fluidGeometry;

    RenderTargetCache m_renderTargetCache;
};
} // namespace crisp