#pragma once

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>

#include <Crisp/Vulkan/VulkanAccelerationStructure.hpp>

namespace crisp
{
class VulkanRayTracingScene : public Scene
{
public:
    VulkanRayTracingScene(Renderer* renderer, Window* window);

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
