#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Math/Distribution2D.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Scenes/PathTracedView.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>

namespace crisp {

class MaterialExplorerScene : public Scene {
public:
    enum class RenderMode : uint8_t { Rasterized, PathTraced, WhiteFurnace };

    MaterialExplorerScene(Renderer* renderer, Window* window, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    void createRenderResources(const std::string& environmentMapName);
    void createSceneObjects(const std::filesystem::path& shaderBallPath);
    void createWhiteFurnaceResources();
    void bindPathTracedEnvironment(bool whiteFurnace);
    void createRayTracedShadowResources();
    void createPathTracedView();
    void setRenderMode(RenderMode mode);
    void updatePresentedImage();
    RenderNode& createRenderNode(std::string_view nodeId);
    PbrMaterialHandle addPbrNode(
        std::string_view nodeId,
        const TriangleMesh& mesh,
        const PbrMaterial& material,
        const glm::mat4& modelMatrix,
        bool castsShadow);

    void setEnvironmentMap(const std::string& environmentMapName);
    void setMaterialPreset(const std::string& materialPresetName);
    void resetMaterial();
    void updateShaderBallShadowMaterials();
    void updateForwardDrawParameters();
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
    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_shadowBlases;
    std::unique_ptr<VulkanAccelerationStructure> m_shadowTlas;

    std::unique_ptr<PathTracedView> m_pathTracedView;
    Distribution2D m_environmentDistribution;
    std::unique_ptr<VulkanImage> m_environmentEquirect;
    std::unique_ptr<VulkanImageView> m_environmentEquirectView;
    glm::uvec2 m_environmentExtent{0, 0};
    std::unique_ptr<VulkanImage> m_whiteFurnaceEnvironmentMap;
    std::unique_ptr<VulkanImage> m_whiteFurnaceEquirect;
    std::unique_ptr<VulkanImageView> m_whiteFurnaceEquirectView;
    Distribution2D m_whiteFurnaceDistribution;
    glm::uvec2 m_whiteFurnaceExtent{0, 0};
    float m_environmentIntensityBeforeFurnace{1.0f};
    std::vector<PathTracedGeometry> m_pathTracedGeometry;
    RenderMode m_renderMode{RenderMode::Rasterized};

    std::array<std::vector<DrawCommand>, kDefaultCascadeCount> m_shadowDrawCommands;
    std::vector<DrawCommand> m_shaderBallForwardDrawCommands;
    std::vector<DrawCommand> m_floorForwardDrawCommands;
    DrawCommand m_skyboxDrawCommand;

    std::vector<RenderNode*> m_shaderBallNodes;
    RenderNode* m_editableMaterialNode{nullptr};
    RenderNode* m_floorNode{nullptr};
    PbrMaterialHandle m_shaderBallMaterialHandle;
    std::optional<PbrMaterialHandle> m_whiteFurnaceMaterialHandle;
    PbrMaterialParams m_shaderBallParams;
    std::unordered_map<RenderNode*, PbrMaterialHandle> m_pbrMaterialHandles;

    std::vector<std::string> m_materialPresetNames;
    std::unordered_map<std::string, PbrMaterial> m_materialPresets;
    std::string m_materialPresetName{"(None)"};
    std::vector<std::string> m_environmentMapNames;
    bool m_showFloor{true};
    bool m_rayTracedShadowsSupported{false};
    bool m_useRayTracedShadows{false};
    bool m_pathTracingSupported{false};
};

} // namespace crisp
