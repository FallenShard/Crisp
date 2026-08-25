#pragma once

#include <array>
#include <memory>
#include <vector>

#include <Crisp/Camera/TargetCameraController.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Materials/PbrMaterialTable.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Scenes/Scene.hpp>
#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>

namespace crisp {

class Geometry;
class Material;
class Skybox;
class TransformBuffer;

class ShadowMappingScene : public Scene {
public:
    ShadowMappingScene(Renderer* renderer, Window* window, const nlohmann::json& args);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
    void drawGui() override;

private:
    static constexpr uint32_t kMaximumObjectCount{64};

    struct ShadowDrawCommand {
        const RenderNode* renderNode{nullptr};
        BoundingBox3 worldBounds;
        DrawCommand command;
    };

    RenderNode& createRenderNode(std::string_view nodeId);
    void createRenderResources(const std::string& environmentMapName);
    void createShowcase();
    void addObject(
        std::string_view nodeId,
        Geometry& geometry,
        const BoundingBox3& localBounds,
        const glm::mat4& modelMatrix,
        const PbrMaterialParams& materialParams,
        bool castsShadow = true);
    void rebuildDrawCommands();
    void setupInput();

    std::unique_ptr<rg::RenderGraph> m_renderGraph;
    std::unique_ptr<TargetCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<Material> m_forwardPassMaterial;
    std::unique_ptr<Material> m_pbrDrawMaterial;
    std::unique_ptr<PbrMaterialTable> m_pbrMaterialTable;
    std::unique_ptr<Skybox> m_skybox;
    std::unique_ptr<VulkanAccelerationStructure> m_descriptorFallbackBlas;
    std::unique_ptr<VulkanAccelerationStructure> m_descriptorFallbackTlas;

    std::array<std::vector<ShadowDrawCommand>, kDefaultCascadeCount> m_shadowDrawCommands;
    std::vector<DrawCommand> m_forwardDrawCommands;
    DrawCommand m_skyboxDrawCommand;

    glm::vec3 m_lightDirection{-1.0f, -1.5f, -0.75f};
    float m_splitLambda{0.5f};
    float m_cascadeBlendFraction{0.1f};
    float m_casterDepthExtrusion{50.0f};
    bool m_visualizeCascades{true};
    bool m_animateLight{false};
};

} // namespace crisp
