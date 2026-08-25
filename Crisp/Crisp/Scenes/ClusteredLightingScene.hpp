#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {

// Must match the Material block in Shaders/physically-based-clustered-lights.frag.glsl.
struct ClusteredMaterialParams {
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.1f};
    float roughness{0.35f};
    int32_t debugMode{0};
};

class ClusteredLightingScene : public Scene {
public:
    ClusteredLightingScene(Renderer* renderer, Window* window, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    void setupInput();
    void createCommonTextures();
    void createSceneObjects();
    RenderNode& createRenderNode(std::string_view id);
    void addDepthPrepassEntry(RenderNode& node, Geometry& geometry);

    void recreateLightClustering();
    void regeneratePointLights();

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;
    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<Skybox> m_skybox;

    Material* m_material{nullptr};
    Material* m_depthMaterial{nullptr};

    ClusteredMaterialParams m_materialParams{};
    int32_t m_pointLightCount{1024};
    bool m_showTileHeatmap{false};
};
} // namespace crisp
