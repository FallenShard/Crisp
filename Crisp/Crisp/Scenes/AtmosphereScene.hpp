#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>

#include <Crisp/Models/Atmosphere.hpp>
#include <Crisp/Models/Tonemap.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {
class AtmosphereScene : public Scene {
public:
    AtmosphereScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    void setupInput();

    void drawAtmosphereGui();

    void drawTonemapGui();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<FreeCameraController> m_cameraController;

    AtmosphereMaterials m_atmosphereMaterials;
    AtmosphereParameters m_atmosphereParams;
    AtmosphereSettings m_settings;
    TonemapParameters m_tonemapParams;
    // In m/s.
    float m_cameraSpeed{1500.0f};
};
} // namespace crisp
