#pragma once

#include <optional>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {
class VulkanRenderPassBuilder {
public:
    // Attachment configuration.
    VulkanRenderPassBuilder& setAttachmentCount(uint32_t count);
    VulkanRenderPassBuilder& setAttachmentDescription(
        uint32_t attachmentIndex, const VkAttachmentDescription& description);
    VulkanRenderPassBuilder& setAttachmentFormat(
        uint32_t attachmentIndex, VkFormat format, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
    VulkanRenderPassBuilder& setAttachmentOps(
        uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
    VulkanRenderPassBuilder& setAttachmentStencilOps(
        uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
    VulkanRenderPassBuilder& setAttachmentLayouts(
        uint32_t attachmentIndex, VkImageLayout initialLayout, VkImageLayout finalLayout);
    VulkanRenderPassBuilder& setAttachmentClearValue(uint32_t attachmentIndex, const VkClearValue& clearValue);
    VulkanRenderPassBuilder& setAttachmentClearColor(uint32_t attachmentIndex, const VkClearColorValue& clearColor);
    VulkanRenderPassBuilder& setAttachmentClearDepthStencil(
        uint32_t attachmentIndex, const VkClearDepthStencilValue& clearDepthStencil);

    // Subpass configuration.
    VulkanRenderPassBuilder& setSubpassCount(uint32_t subpassCount);
    VulkanRenderPassBuilder& setSubpassDescription(
        uint32_t subpass,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        VkSubpassDescriptionFlags flags = 0);
    VulkanRenderPassBuilder& addInputAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    VulkanRenderPassBuilder& addColorAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, std::optional<VkImageLayout> imageLayout = std::nullopt);
    VulkanRenderPassBuilder& addResolveAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    VulkanRenderPassBuilder& setDepthAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, std::optional<VkImageLayout> imageLayout = std::nullopt);
    VulkanRenderPassBuilder& addPreserveAttachmentRef(uint32_t subpass, uint32_t attachmentIndex);

    VulkanRenderPassBuilder& addDependency(
        uint32_t srcSubpass,
        uint32_t dstSubpass,
        VkPipelineStageFlags srcStageMask,
        VkAccessFlags srcAccessMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        VkDependencyFlags flags = 0);

    VulkanRenderPassBuilder& addDependency(
        uint32_t srcSubpass, uint32_t dstSubpass, const VulkanSynchronizationScope& scope, VkDependencyFlags flags = 0);

    std::unique_ptr<VulkanRenderPass> create(const VulkanDevice& device, const VkExtent2D& renderArea) const;

private:
    std::vector<VkAttachmentDescription> m_attachments;
    std::vector<uint32_t> m_clearedAttachmentIndices;
    std::vector<VkClearValue> m_clearValues;

    std::vector<std::vector<VkAttachmentReference>> m_inputAttachmentRefs;
    std::vector<std::vector<VkAttachmentReference>> m_colorAttachmentRefs;
    std::vector<std::vector<VkAttachmentReference>> m_resolveAttachmentRefs;
    std::vector<VkAttachmentReference> m_depthAttachmentRefs;
    std::vector<std::vector<uint32_t>> m_preserveAttachments;
    std::vector<VkSubpassDescription> m_subpasses;
    std::vector<VkSubpassDependency> m_dependencies;
};
} // namespace crisp