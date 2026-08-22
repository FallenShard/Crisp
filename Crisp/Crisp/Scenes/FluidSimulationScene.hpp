#pragma once

#include <memory>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Geometry/TransformPack.hpp>
#include <Crisp/Models/SPH.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {

class FluidSimulationScene : public Scene {
public:
    FluidSimulationScene(Renderer* renderer, Window* window);
    ~FluidSimulationScene() override;

    FluidSimulationScene(FluidSimulationScene&&) = delete;
    FluidSimulationScene& operator=(FluidSimulationScene&&) = delete;
    FluidSimulationScene(const FluidSimulationScene&) = delete;
    FluidSimulationScene& operator=(const FluidSimulationScene&) = delete;

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

    const SPH& getSimulation() const {
        return *m_fluidSimulation;
    }

private:
    // Mirrors the ParticleParams block in point-sphere-sprite.{vert,frag}.glsl.
    struct ParticleParams {
        float radius;
        float screenSpaceScale;
    };

    void setupInput();
    void drawStageTimings() const;
    void buildRenderGraph();
    void resetCamera();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<SPH> m_fluidSimulation;

    Geometry* m_particleGeometry{nullptr};
    VulkanPipeline* m_pointSpritePipeline{nullptr};
    Material* m_pointSpriteMaterial{nullptr};

    TransformPack m_transforms{};
    ParticleParams m_particleParams{};

    float m_vizTimeDelta{0.0f};
    float m_vizScale{10.0f};
};
} // namespace crisp
