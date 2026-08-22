#pragma once

#include <array>

#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {

class MaterialExplorerScene : public Scene {
public:
    MaterialExplorerScene(Renderer* renderer, Window* window, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    void createRenderResources(const std::string& environmentMapName);
    void createSceneObjects(const std::filesystem::path& shaderBallPath);
    RenderNode& createRenderNode(std::string_view nodeId);
    PbrMaterialHandle addPbrNode(
        std::string_view nodeId,
        const TriangleMesh& mesh,
        const PbrMaterial& material,
        const glm::mat4& modelMatrix,
        bool castsShadow);

    void setEnvironmentMap(const std::string& environmentMapName);
    void resetMaterial();
    void updateShaderBallShadowMaterials();
    void rebuildDrawCommands();
    void setupInput();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<TargetCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<Material> m_forwardPassMaterial;
    std::unique_ptr<Material> m_pbrDrawMaterial;
    std::unique_ptr<Material> m_pbrDoubleSidedDrawMaterial;
    std::unique_ptr<PbrMaterialTable> m_pbrMaterialTable;
    std::unique_ptr<Skybox> m_skybox;

    std::array<std::vector<DrawCommand>, kDefaultCascadeCount> m_shadowDrawCommands;
    std::vector<DrawCommand> m_shaderBallForwardDrawCommands;
    std::vector<DrawCommand> m_floorForwardDrawCommands;
    DrawCommand m_skyboxDrawCommand;

    std::vector<RenderNode*> m_shaderBallNodes;
    RenderNode* m_editableMaterialNode{nullptr};
    RenderNode* m_floorNode{nullptr};
    PbrMaterialHandle m_shaderBallMaterialHandle;
    PbrParams m_shaderBallParams;

    std::vector<std::string> m_environmentMapNames;
    bool m_showFloor{true};
};

} // namespace crisp
