#pragma once

#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {

class Skybox {
public:
    Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, const std::string& cubeMapFolder);
    Skybox(
        Renderer* renderer,
        const VulkanRenderPass& renderPass,
        const VulkanImageView& cubeMapView,
        const VulkanSampler& sampler);

    void updateTransforms(const glm::mat4& V, const glm::mat4& P, uint32_t regionIndex);

    const RenderNode& getRenderNode();

    VulkanImageView* getSkyboxView() const;

    void updateDeviceBuffer(VkCommandBuffer cmdBuffer);

private:
    void updateRenderNode(const VulkanSampler& sampler, const VulkanImageView& cubeMapView);

    Geometry m_cubeGeometry;

    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<Material> m_material;

    TransformPack m_transformPack;
    std::unique_ptr<VulkanRingBuffer> m_transformBuffer;

    std::unique_ptr<VulkanImage> m_cubeMap;
    std::unique_ptr<VulkanImageView> m_cubeMapView;
    std::unique_ptr<VulkanSampler> m_sampler;

    RenderNode m_renderNode;
};
} // namespace crisp