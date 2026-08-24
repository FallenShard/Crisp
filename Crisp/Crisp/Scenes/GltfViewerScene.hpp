#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>

namespace crisp {

struct GltfLoadReport {
    std::filesystem::path path;
    bool succeeded{false};

    std::string error;

    uint32_t modelCount{0};
    uint32_t imageCount{0};
    uint32_t vertexCount{0};
    uint32_t triangleCount{0};
    uint32_t submeshCount{0};
    uint32_t skinnedModelCount{0};
    uint32_t animationCount{0};
    uint32_t alphaMaskedCount{0};
    uint32_t doubleSidedCount{0};
    BoundingBox3 bounds;
};

class GltfViewerScene : public Scene {
public:
    GltfViewerScene(Renderer* renderer, Window* window, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    static constexpr uint32_t kMaximumObjectCount = 4096;

    RenderNode& createRenderNode(std::string_view nodeId, bool hasTransform = true);
    void createCommonTextures();
    void setEnvironmentMap(const std::string& envMapName);

    void loadAsset(const std::filesystem::path& path);
    void addSceneObject(
        std::string_view nodeId, const TriangleMesh& mesh, const PbrMaterial& material, const glm::mat4& modelMatrix);
    void createRayTracedShadowResources();
    void updateForwardDrawParameters();
    void frameCameraOnBounds(const BoundingBox3& bounds);
    void rebuildDrawCommandCache();
    void setupInput();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<TargetCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<Material> m_forwardPassMaterial;
    std::unique_ptr<Material> m_pbrDrawMaterial;
    std::unique_ptr<PbrMaterialTable> m_pbrMaterialTable;
    std::unique_ptr<Skybox> m_skybox;
    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_shadowBlases;
    std::unique_ptr<VulkanAccelerationStructure> m_shadowTlas;

    FlatStringHashMap<std::unique_ptr<RenderNode>> m_renderNodes;
    FlatHashMap<const RenderNode*, BoundingBox3> m_renderNodeWorldBounds;

    struct CachedDrawCommand {
        const RenderNode* renderNode{nullptr};
        BoundingBox3 worldBounds;
        DrawCommand command;
    };

    std::array<std::vector<CachedDrawCommand>, kDefaultCascadeCount + 1> m_drawCommandCache{};

    FlatHashMap<RenderNode*, PbrMaterialHandle> m_pbrMaterialHandles;
    bool m_rayTracedShadowsSupported{false};
    bool m_useRayTracedShadows{false};

    GltfLoadReport m_report;
    std::vector<std::string> m_environmentMapNames;
};

} // namespace crisp
