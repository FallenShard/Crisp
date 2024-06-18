#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Lights/EnvironmentLight.hpp>
#include <Crisp/Models/Ocean.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {
class OceanScene : public Scene {
public:
    OceanScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;
    void renderGui() override;

private:
    std::unique_ptr<VulkanImage> createInitialSpectrum();
    int applyFFT(std::string image);
    void buildNewFFT();

    std::unique_ptr<rg::RenderGraph> m_rg;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::vector<std::unique_ptr<RenderNode>> m_renderNodes;

    std::unique_ptr<EnvironmentLight> m_envLight;
    std::unique_ptr<Skybox> m_skybox;

    OceanParameters m_oceanParams;
    float m_choppiness;

    bool m_paused{false};
};
} // namespace crisp
