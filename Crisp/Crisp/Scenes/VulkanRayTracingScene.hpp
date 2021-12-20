#pragma once

#include "Scene.hpp"
#include <Crisp/Renderer/Renderer.hpp>

#include <CrispCore/Math/Headers.hpp>
#include <CrispCore/RobinHood.hpp>

namespace crisp
{
class Application;

class FreeCameraController;
class TransformBuffer;
class LightSystem;

class ResourceContext;
class RenderGraph;
struct RenderNode;

class VulkanImage;
class VulkanImageView;
class VUlkanPipeline;
class VulkanPipelineLayout;
class VulkanAccelerationStructure;
class RayTracingMaterial;

class VulkanRayTracingScene : public AbstractScene
{
public:
    VulkanRayTracingScene(Renderer* renderer, Application* app);
    ~VulkanRayTracingScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
    virtual void render() override;

private:
    struct PbrUnifMaterialParams
    {
        glm::vec4 albedo;
        float metallic;
        float roughness;
    };

    RenderNode* createRenderNode(std::string id, int transformIndex);

    void createPlane();
    std::unique_ptr<VulkanPipelineLayout> createPipelineLayout();
    void createPipeline(std::unique_ptr<VulkanPipelineLayout> pipelineLayout);

    void setupInput();
    void createGui();

    void updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx);

    Renderer* m_renderer;
    Application* m_app;

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<RenderGraph> m_renderGraph;
    std::unique_ptr<ResourceContext> m_resourceContext;
    robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_bottomLevelAccelStructures;
    std::unique_ptr<VulkanAccelerationStructure> m_topLevelAccelStructure;

    std::unique_ptr<VulkanImage> m_rtImage;
    std::vector<std::unique_ptr<VulkanImageView>> m_rtImageViews;

    std::unique_ptr<VulkanPipeline> m_pipeline;

    std::vector<std::vector<VkDescriptorSet>> m_descSets;
    std::unique_ptr<VulkanBuffer> m_sbtBuffer;

    uint32_t m_frameIdx{ 0 };
};
} // namespace crisp
