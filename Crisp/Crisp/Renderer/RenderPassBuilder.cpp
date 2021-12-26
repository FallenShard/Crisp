#include "RenderPassBuilder.hpp"

namespace crisp
{
RenderPassBuilder::RenderPassBuilder() {}

RenderPassBuilder& RenderPassBuilder::addAttachment(VkFormat format, VkSampleCountFlagBits samples)
{
    VkAttachmentDescription attachment = {};
    attachment.format = format;
    attachment.samples = samples;
    m_attachments.push_back(attachment);
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentOps(uint32_t attachmentIndex, VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp)
{
    m_attachments[attachmentIndex].loadOp = loadOp;
    m_attachments[attachmentIndex].storeOp = storeOp;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentStencilOps(uint32_t attachmentIndex, VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp)
{
    m_attachments[attachmentIndex].stencilLoadOp = loadOp;
    m_attachments[attachmentIndex].stencilStoreOp = storeOp;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentLayouts(uint32_t attachmentIndex, VkImageLayout initialLayout,
    VkImageLayout finalLayout)
{
    m_attachments[attachmentIndex].initialLayout = initialLayout;
    m_attachments[attachmentIndex].finalLayout = finalLayout;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setNumSubpasses(uint32_t numSubpasses)
{
    m_inputAttachmentRefs.resize(numSubpasses);
    m_colorAttachmentRefs.resize(numSubpasses);
    m_resolveAttachmentRefs.resize(numSubpasses);
    m_depthAttachmentRefs.resize(numSubpasses);
    m_preserveAttachments.resize(numSubpasses);
    m_subpasses.resize(numSubpasses, VkSubpassDescription{ 0, VK_PIPELINE_BIND_POINT_GRAPHICS });
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setSubpassDescription(uint32_t subpass, VkPipelineBindPoint bindPoint,
    VkSubpassDescriptionFlags flags)
{
    m_subpasses[subpass] = { flags, bindPoint };
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addInputAttachmentRef(uint32_t subpass, uint32_t attachment,
    VkImageLayout imageLayout)
{
    m_inputAttachmentRefs[subpass].push_back({ attachment, imageLayout });
    m_subpasses[subpass].inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentRefs[subpass].size());
    m_subpasses[subpass].pInputAttachments = m_inputAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addColorAttachmentRef(uint32_t subpass, uint32_t attachment,
    VkImageLayout imageLayout)
{
    m_colorAttachmentRefs[subpass].push_back({ attachment, imageLayout });
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addResolveAttachmentRef(uint32_t subpass, uint32_t attachment,
    VkImageLayout imageLayout)
{
    m_resolveAttachmentRefs[subpass].push_back({ attachment, imageLayout });
    m_subpasses[subpass].pResolveAttachments = m_resolveAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setDepthAttachmentRef(uint32_t subpass, uint32_t attachment,
    VkImageLayout imageLayout)
{
    m_depthAttachmentRefs[subpass] = { attachment, imageLayout };
    m_subpasses[subpass].pDepthStencilAttachment = &m_depthAttachmentRefs[subpass];
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addPreserveAttachmentRef(uint32_t subpass, uint32_t attachment)
{
    m_preserveAttachments[subpass].push_back(attachment);
    m_subpasses[subpass].preserveAttachmentCount = static_cast<uint32_t>(m_preserveAttachments[subpass].size());
    m_subpasses[subpass].pPreserveAttachments = m_preserveAttachments[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addDependency(uint32_t srcSubpass, uint32_t dstSubpass,
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask, VkDependencyFlags flags)
{
    m_dependencies.push_back(
        { srcSubpass, dstSubpass, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, flags });
    return *this;
}

std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> RenderPassBuilder::create(VkDevice device)
{
    VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
    renderPassInfo.pAttachments = m_attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    renderPassInfo.pSubpasses = m_subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(m_dependencies.size());
    renderPassInfo.pDependencies = m_dependencies.data();

    VkRenderPass renderPass;
    vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    return std::make_pair(renderPass, m_attachments);
}
} // namespace crisp