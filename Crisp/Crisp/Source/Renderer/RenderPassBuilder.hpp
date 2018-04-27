#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    class RenderPassBuilder
    {
    public:
        RenderPassBuilder();

        RenderPassBuilder& addAttachment(VkFormat format, VkSampleCountFlagBits samples);
        RenderPassBuilder& setAttachmentOps(uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
        RenderPassBuilder& setAttachmentStencilOps(uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
        RenderPassBuilder& setAttachmentLayouts(uint32_t attachmentIndex, VkImageLayout initialLayout, VkImageLayout finalLayout);

        RenderPassBuilder& addSubpass(VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, VkSubpassDescriptionFlags flags = 0);
        RenderPassBuilder& addInputAttachmentRef(uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout);
        RenderPassBuilder& addColorAttachmentRef(uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout);
        RenderPassBuilder& addResolveAttachmentRef(uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout);
        RenderPassBuilder& setDepthAttachmentRef(uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout);
        RenderPassBuilder& addPreserveAttachmentRef(uint32_t subpass, uint32_t attachment);

        RenderPassBuilder& addDependency(uint32_t srcSubpass, uint32_t dstSubpass, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
            VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkDependencyFlags flags = 0);

        VkRenderPass create(VkDevice device);

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
}