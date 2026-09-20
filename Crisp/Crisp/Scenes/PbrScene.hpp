#pragma once

#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipelineStatsQueryPool.hpp>

namespace crisp {
class PbrScene : public Scene {
public:
    PbrScene(Renderer* renderer, Window* window, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    static constexpr uint32_t kMaximumObjectCount = 4096;

    RenderNode& createRenderNode(std::string_view nodeId, bool hasTransform = true);

    void createCommonTextures();
    void setEnvironmentMap(const std::string& envMapName);

    void createSceneObjects(const std::filesystem::path& path);
    void createGltfSceneObjects(const std::filesystem::path& path);
    void createObjSceneObject(const std::filesystem::path& path);
    void addSceneObject(
        std::string_view nodeId,
        const TriangleMesh& mesh,
        const PbrMaterial& material,
        const glm::mat4& modelMatrix,
        Geometry* sharedGeometry = nullptr,
        int32_t geometryPartIndex = -1);
    void createPlane();
    void createMeshletTestNode();
    void rebuildDrawCommandCache();

    void setupInput();

    static constexpr uint32_t kStatsPassCount = kDefaultCascadeCount + 1;
    static constexpr uint32_t kForwardStatsPass = kDefaultCascadeCount;

    static uint32_t getStatsQueryIndex(const uint32_t virtualFrameIndex, const uint32_t passIndex) {
        return virtualFrameIndex * kStatsPassCount + passIndex;
    }

    void beginPipelineStatsFrame(uint32_t virtualFrameIndex);
    bool shouldRecordPipelineStats(uint32_t queryIndex) const;

    bool m_mergeGeometry{true};
    bool m_optimizeIndices{true};
    int32_t m_nodesToDraw = 0;
    std::unique_ptr<rg::RenderGraph> m_renderGraph;

    std::unique_ptr<TargetCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    float m_cascadeBlendFraction{0.1f};
    float m_casterDepthExtrusion{50.0f};
    bool m_visualizeCascades{false};

    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatStringHashMap<std::unique_ptr<RenderNode>> m_renderNodes;
    FlatHashMap<const RenderNode*, BoundingBox3> m_renderNodeWorldBounds;

    std::unique_ptr<Material> m_forwardPassMaterial;
    std::unique_ptr<Material> m_pbrDrawMaterial;
    std::unique_ptr<PbrMaterialTable> m_pbrMaterialTable;

    std::unique_ptr<Skybox> m_skybox;

    std::vector<std::string> m_environmentMapNames;

    bool m_showFloor{true};

    bool m_drawMeshlets{false};
    MeshletData m_meshletData;

    struct CachedDrawCommand {
        const RenderNode* renderNode{nullptr};
        uint32_t nodeIndex{0};
        BoundingBox3 worldBounds;
        DrawCommand command;
    };

    std::array<std::vector<CachedDrawCommand>, kDefaultCascadeCount + 1> m_drawCommandCache{};

    struct DrawStats {
        std::array<uint32_t, kDefaultCascadeCount> cascadeConsidered{};
        std::array<uint32_t, kDefaultCascadeCount> cascadeRecorded{};
        uint32_t forwardRecorded{0};
        uint32_t frameCounter{0};

        void report();
    };

    DrawStats m_drawStats{};

    std::unique_ptr<VulkanPipelineStatsQueryPool> m_pipelineStatsQueryPool;
    std::array<PipelineStats, kStatsPassCount> m_pipelineStats{};
    bool m_collectPipelineStats{false};
};
} // namespace crisp
