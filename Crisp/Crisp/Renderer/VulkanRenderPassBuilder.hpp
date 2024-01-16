#pragma once

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <vector>

namespace crisp {
class VulkanRenderPassBuilder {
public:
    // Attachment configuration
    VulkanRenderPassBuilder& setAttachmentCount(uint32_t count);
    VulkanRenderPassBuilder& setAttachment(uint32_t attachmentIndex, const VkAttachmentDescription& description);

    // Subpass configuration
    VulkanRenderPassBuilder& setSubpassCount(uint32_t numSubpasses);
    VulkanRenderPassBuilder& configureSubpass(
        uint32_t subpass,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        VkSubpassDescriptionFlags flags = 0);

    VulkanRenderPassBuilder& addInputAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    VulkanRenderPassBuilder& addColorAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
    VulkanRenderPassBuilder& addColorAttachmentRef(uint32_t subpass, uint32_t attachmentIndex);
    VulkanRenderPassBuilder& addResolveAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);

    VulkanRenderPassBuilder& setDepthAttachmentRef(
        uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);

    VulkanRenderPassBuilder& addPreserveAttachmentRef(uint32_t subpass, uint32_t attachmentIndex);

    VulkanRenderPassBuilder& addDependency(
        uint32_t srcSubpass,
        uint32_t dstSubpass,
        VkPipelineStageFlags srcStageMask,
        VkAccessFlags srcAccessMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        VkDependencyFlags flags = 0);

    VulkanRenderPassBuilder& addDependency(const VkSubpassDependency& dependency) {
        m_dependencies.emplace_back(dependency);
        return *this;
    }

    std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> create(VkDevice device) const;
    std::vector<VkImageLayout> getFinalLayouts() const;

private:
    std::vector<VkAttachmentDescription> m_attachments;

    std::vector<std::vector<VkAttachmentReference>> m_inputAttachmentRefs;
    std::vector<std::vector<VkAttachmentReference>> m_colorAttachmentRefs;
    std::vector<std::vector<VkAttachmentReference>> m_resolveAttachmentRefs;
    std::vector<VkAttachmentReference> m_depthAttachmentRefs;
    std::vector<std::vector<uint32_t>> m_preserveAttachments;
    std::vector<VkSubpassDescription> m_subpasses;

    std::vector<VkSubpassDependency> m_dependencies;
};
} // namespace crisp