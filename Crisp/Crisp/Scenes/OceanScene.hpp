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
struct OceanPassResources;

class OceanScene : public Scene {
public:
    OceanScene(Renderer* renderer, Window* window);
    ~OceanScene() override;

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    void setupInput();
    void setupResources();
    void buildRenderGraph();
    void resetCamera();

    std::unique_ptr<VulkanImage> createInitialSpectrum();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<OceanPassResources> m_passResources;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<TransformBuffer> m_transformBuffer;
    TransformHandle m_transformHandle{TransformHandle::createInvalidHandle()};
    VulkanPipeline* m_oceanPipeline{nullptr};
    Material* m_oceanMaterial{nullptr};

    std::unique_ptr<EnvironmentLight> m_envLight;
    std::unique_ptr<Skybox> m_skybox;
    OceanParameters m_oceanParams;
    float m_choppiness;

    // Uniform scale on the ocean's model matrix, so a patch can be inspected as an object.
    float m_modelScale{1.0f};

    // Instances per side of the tiled ocean grid, centred on the origin. 1 == a single patch.
    int32_t m_instancesPerSide{1};

    bool m_paused{false};
};
} // namespace crisp
