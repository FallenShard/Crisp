#pragma once

#include "Scene.hpp"
#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Common/HashMap.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp
{
class Application;

class FreeCameraController;
class TransformBuffer;
class LightSystem;

struct RenderNode;

class VulkanImage;
class VulkanImageView;
class VUlkanPipeline;
class VulkanPipelineLayout;
class VulkanAccelerationStructure;

class VulkanRayTracingScene : public AbstractScene
{
public:
    VulkanRayTracingScene(Renderer* renderer, Application* app);
    ~VulkanRayTracingScene() override = default;

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

    std::unique_ptr<VulkanPipelineLayout> createPipelineLayout();
    std::unique_ptr<VulkanPipeline> createPipeline(std::unique_ptr<VulkanPipelineLayout> pipelineLayout);
    void updateDescriptorSets();

    void setupInput();

    void updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx);

    std::unique_ptr<FreeCameraController> m_cameraController;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<TransformBuffer> m_transformBuffer;

    FlatHashMap<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_bottomLevelAccelStructures;
    std::unique_ptr<VulkanAccelerationStructure> m_topLevelAccelStructure;

    std::unique_ptr<VulkanImage> m_rtImage;
    std::vector<std::unique_ptr<VulkanImageView>> m_rtImageViews;

    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<Material> m_material;

    struct ShaderBindingTable
    {
        std::unique_ptr<VulkanBuffer> buffer;

        VkStridedDeviceAddressRegionKHR rgen;
        VkStridedDeviceAddressRegionKHR miss;
        VkStridedDeviceAddressRegionKHR hit;
        VkStridedDeviceAddressRegionKHR call;
    };

    ShaderBindingTable m_sbt;

    uint32_t m_frameIdx{0};
};
} // namespace crisp
