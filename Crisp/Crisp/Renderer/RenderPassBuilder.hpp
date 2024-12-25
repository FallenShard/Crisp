#pragma once

#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

#include <vector>

namespace crisp {
class RenderPassBuilder {
public:
    RenderPassBuilder& setAllocateAttachmentViews(bool allocateAttachmentViews);

    // Attachment configuration
    RenderPassBuilder& setAttachmentCount(uint32_t count);
    RenderPassBuilder& setAttachmentMapping(
        uint32_t attachmentIdx,
        uint32_t renderTargetIdx,
        uint32_t firstLayer = 0,
        uint32_t layerCount = 1,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevelCount = 1);
    RenderPassBuilder& setAttachmentBufferOverDepthSlices(uint32_t attachmentIndex, bool bufferOverDepth);
    RenderPassBuilder& setAttachmentOps(
        uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
    RenderPassBuilder& setAttachmentStencilOps(
        uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
    RenderPassBuilder& setAttachmentLayouts(
        uint32_t attachmentIndex, VkImageLayout initialLayout, VkImageLayout finalLayout);

    // Subpass configuration
    RenderPassBuilder& setNumSubpasses(uint32_t numSubpasses);
    RenderPassBuilder& setSubpassDescription(
        uint32_t subpass,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        VkSubpassDescriptionFlags flags = 0);
    RenderPassBuilder& addInputAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    RenderPassBuilder& addColorAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    RenderPassBuilder& addResolveAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    RenderPassBuilder& setDepthAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    RenderPassBuilder& addPreserveAttachmentRef(uint32_t subpass, uint32_t attachmentIndex);

    RenderPassBuilder& addDependency(
        uint32_t srcSubpass,
        uint32_t dstSubpass,
        VkPipelineStageFlags srcStageMask,
        VkAccessFlags srcAccessMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        VkDependencyFlags flags = 0);

    std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> create(
        VkDevice device, std::vector<RenderTarget*> renderTargets) const;

    std::unique_ptr<VulkanRenderPass> create(
        const VulkanDevice& device, VkExtent2D renderArea, const std::vector<RenderTarget*>& renderTargets) const;

private:
    std::vector<VkAttachmentDescription> m_attachments;
    std::vector<AttachmentMapping> m_attachmentMappings;

    std::vector<std::vector<VkAttachmentReference>> m_inputAttachmentRefs;
    std::vector<std::vector<VkAttachmentReference>> m_colorAttachmentRefs;
    std::vector<std::vector<VkAttachmentReference>> m_resolveAttachmentRefs;
    std::vector<VkAttachmentReference> m_depthAttachmentRefs;
    std::vector<std::vector<uint32_t>> m_preserveAttachments;
    std::vector<VkSubpassDescription> m_subpasses;
    std::vector<VkSubpassDependency> m_dependencies;

    bool m_allocateAttachmentViews{true};
};
} // namespace crisp