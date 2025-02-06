#pragma once

#include <optional>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {
struct RenderPassCreationParams {
    std::vector<VkClearValue> clearValues;
};

class RenderPassBuilder {
public:
    RenderPassBuilder& setAllocateAttachmentViews(bool allocateAttachmentViews);
    RenderPassBuilder& setSwapChainDependent(bool isSwapChainDependent);

    // Attachment configuration
    RenderPassBuilder& setAttachmentCount(uint32_t count);
    RenderPassBuilder& setAttachmentDescription(uint32_t attachmentIndex, const VkAttachmentDescription& description);
    RenderPassBuilder& setAttachmentFormat(
        uint32_t attachmentIndex, VkFormat format, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
    RenderPassBuilder& setAttachmentOps(
        uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
    RenderPassBuilder& setAttachmentStencilOps(
        uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
    RenderPassBuilder& setAttachmentLayouts(
        uint32_t attachmentIndex, VkImageLayout initialLayout, VkImageLayout finalLayout);

    // Subpass configuration
    RenderPassBuilder& setSubpassCount(uint32_t subpassCount);
    RenderPassBuilder& setSubpassDescription(
        uint32_t subpass,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        VkSubpassDescriptionFlags flags = 0);
    RenderPassBuilder& addInputAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    RenderPassBuilder& addColorAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, std::optional<VkImageLayout> imageLayout = std::nullopt);
    RenderPassBuilder& addResolveAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    RenderPassBuilder& setDepthAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, std::optional<VkImageLayout> imageLayout = std::nullopt);
    RenderPassBuilder& addPreserveAttachmentRef(uint32_t subpass, uint32_t attachmentIndex);

    RenderPassBuilder& addDependency(
        uint32_t srcSubpass,
        uint32_t dstSubpass,
        VkPipelineStageFlags srcStageMask,
        VkAccessFlags srcAccessMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        VkDependencyFlags flags = 0);

    RenderPassBuilder& addDependency(
        uint32_t srcSubpass, uint32_t dstSubpass, const VulkanSynchronizationScope& scope, VkDependencyFlags flags = 0);

    std::unique_ptr<VulkanRenderPass> create(
        const VulkanDevice& device, VkExtent2D renderArea, const RenderPassCreationParams& creationParams) const;

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
    bool m_isSwapChainDependent{false};
};
} // namespace crisp