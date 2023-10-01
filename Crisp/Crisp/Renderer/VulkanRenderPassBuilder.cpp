#include <Crisp/Renderer/VulkanRenderPassBuilder.hpp>

#include <Crisp/Vulkan/VulkanChecks.hpp>

namespace crisp {

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentCount(const uint32_t count) {
    m_attachments.resize(count);
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachment(
    const int32_t attachmentIndex, const VkAttachmentDescription& description) {
    m_attachments.at(attachmentIndex) = description;
    return *this;
}

// Subpass configuration
VulkanRenderPassBuilder& VulkanRenderPassBuilder::setSubpassCount(const uint32_t numSubpasses) {
    m_inputAttachmentRefs.resize(numSubpasses);
    m_colorAttachmentRefs.resize(numSubpasses);
    m_resolveAttachmentRefs.resize(numSubpasses);
    m_depthAttachmentRefs.resize(numSubpasses);
    m_preserveAttachments.resize(numSubpasses);
    m_subpasses.resize(numSubpasses, VkSubpassDescription{0, VK_PIPELINE_BIND_POINT_GRAPHICS});
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::configureSubpass(
    const uint32_t subpass, const VkPipelineBindPoint bindPoint, const VkSubpassDescriptionFlags flags) {
    m_subpasses.at(subpass) = {flags, bindPoint};
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addInputAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout) {
    m_inputAttachmentRefs[subpass].push_back({attachmentIndex, imageLayout});
    m_subpasses[subpass].inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentRefs[subpass].size());
    m_subpasses[subpass].pInputAttachments = m_inputAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addColorAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout) {
    m_colorAttachmentRefs[subpass].push_back({attachmentIndex, imageLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addColorAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex) {
    m_colorAttachmentRefs[subpass].push_back({attachmentIndex, m_attachments.at(attachmentIndex).finalLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addResolveAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout) {
    m_resolveAttachmentRefs[subpass].push_back({attachmentIndex, imageLayout});
    m_subpasses[subpass].pResolveAttachments = m_resolveAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setDepthAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout) {
    m_depthAttachmentRefs[subpass] = {attachmentIndex, imageLayout};
    m_subpasses[subpass].pDepthStencilAttachment = &m_depthAttachmentRefs[subpass];
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addPreserveAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex) {
    m_preserveAttachments[subpass].push_back(attachmentIndex);
    m_subpasses[subpass].preserveAttachmentCount = static_cast<uint32_t>(m_preserveAttachments[subpass].size());
    m_subpasses[subpass].pPreserveAttachments = m_preserveAttachments[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addDependency(
    const uint32_t srcSubpass,
    const uint32_t dstSubpass,
    const VkPipelineStageFlags srcStageMask,
    const VkAccessFlags srcAccessMask,
    const VkPipelineStageFlags dstStageMask,
    const VkAccessFlags dstAccessMask,
    const VkDependencyFlags flags) {
    m_dependencies.push_back({srcSubpass, dstSubpass, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, flags});
    return *this;
}

std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> VulkanRenderPassBuilder::create(
    const VkDevice device) const {
    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
    renderPassInfo.pAttachments = m_attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    renderPassInfo.pSubpasses = m_subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(m_dependencies.size());
    renderPassInfo.pDependencies = m_dependencies.data();

    VkRenderPass renderPass{VK_NULL_HANDLE};
    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
    return {renderPass, m_attachments};
}

std::vector<VkImageLayout> VulkanRenderPassBuilder::getFinalLayouts() const {
    std::vector<VkImageLayout> layouts;
    layouts.reserve(m_attachments.size());
    for (const auto& attachment : m_attachments) {
        layouts.push_back(attachment.finalLayout);
    }
    return layouts;
}
} // namespace crisp