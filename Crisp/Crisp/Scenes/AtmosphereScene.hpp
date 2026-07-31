#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>

#include <Crisp/Models/Atmosphere.hpp>
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
    struct AtmosphereSettings {
        float sunAzimuthDegrees{270.0f};
        float sunElevationDegrees{25.8f};
        glm::vec3 sunColor{1.0f, 1.0f, 1.0f};
        float sunIrradianceScale{1.0f};

        float rayleighScaleHeight{kEarthRayleighScaleHeight};
        float mieScaleHeight{kEarthMieScaleHeight};
        float atmosphereHeight{100.0f};
    };

    void setupInput();

    void applyAtmosphereSettings();

    void drawAtmosphereGui();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<FreeCameraController> m_cameraController;

    AtmosphereParameters m_atmosphereParams;
    AtmosphereSettings m_settings;
    // In m/s.
    float m_cameraSpeed{1500.0f};
};
} // namespace crisp
