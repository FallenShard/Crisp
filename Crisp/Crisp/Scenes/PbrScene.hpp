#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {
class PbrScene : public Scene {
public:
    PbrScene(Renderer* renderer, Window* window, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

    void onMaterialSelected(const std::string& material);

private:
    RenderNode* createRenderNode(std::string_view nodeId, bool hasTransform);

    void createCommonTextures();
    void setEnvironmentMap(const std::string& envMapName);

    void createSceneObject(const std::filesystem::path& path);
    void createPlane();

    void setupInput();

    void updateRenderPassMaterials();
    void updateSceneViews();

    int32_t m_nodesToDraw = 2500;
    std::unique_ptr<rg::RenderGraph> m_rg;

    std::unique_ptr<TargetCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;

    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatStringHashMap<std::unique_ptr<RenderNode>> m_renderNodes;

    std::unique_ptr<Material> m_forwardPassMaterial;

    PbrParams m_uniformMaterialParams;
    std::unique_ptr<Skybox> m_skybox;

    std::string m_shaderBallPbrMaterialKey;

    std::vector<std::string> m_environmentMapNames;

    bool m_showFloor{true};
};
} // namespace crisp
