#include <Crisp/Vulkan/VulkanRenderPassBuilder.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

namespace crisp {

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentCount(const uint32_t count) {
    m_attachments.resize(count);
    m_clearedAttachmentIndices.resize(count, ~0u);
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentDescription(
    const uint32_t attachmentIndex, const VkAttachmentDescription& description) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex] = description;

    if (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        m_clearedAttachmentIndices[attachmentIndex] = static_cast<uint32_t>(m_clearValues.size());
        m_clearValues.emplace_back();
    }
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentFormat(
    const uint32_t attachmentIndex, const VkFormat format, const VkSampleCountFlagBits sampleCount) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].format = format;
    m_attachments[attachmentIndex].samples = sampleCount;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentOps(
    const uint32_t attachmentIndex, const VkAttachmentLoadOp loadOp, const VkAttachmentStoreOp storeOp) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].loadOp = loadOp;
    m_attachments[attachmentIndex].storeOp = storeOp;

    if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        m_clearedAttachmentIndices[attachmentIndex] = static_cast<uint32_t>(m_clearValues.size());
        m_clearValues.emplace_back();
    }
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentStencilOps(
    const uint32_t attachmentIndex, const VkAttachmentLoadOp loadOp, const VkAttachmentStoreOp storeOp) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].stencilLoadOp = loadOp;
    m_attachments[attachmentIndex].stencilStoreOp = storeOp;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentLayouts(
    const uint32_t attachmentIndex, const VkImageLayout initialLayout, const VkImageLayout finalLayout) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].initialLayout = initialLayout;
    m_attachments[attachmentIndex].finalLayout = finalLayout;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentClearValue(
    uint32_t attachmentIndex, const VkClearValue& clearValue) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_clearedAttachmentIndices.size());
    const uint32_t clearIndex = m_clearedAttachmentIndices[attachmentIndex];
    CRISP_CHECK_GE_LT(clearIndex, 0, m_clearValues.size());
    m_clearValues[clearIndex] = clearValue;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentClearColor(
    const uint32_t attachmentIndex, const VkClearColorValue& clearColor) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_clearedAttachmentIndices.size());
    const uint32_t clearIndex = m_clearedAttachmentIndices[attachmentIndex];
    CRISP_CHECK_GE_LT(clearIndex, 0, m_clearValues.size());
    m_clearValues[clearIndex].color = clearColor;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentClearDepthStencil(
    uint32_t attachmentIndex, const VkClearDepthStencilValue& clearDepthStencil) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_clearedAttachmentIndices.size());
    const uint32_t clearIndex = m_clearedAttachmentIndices[attachmentIndex];
    CRISP_CHECK_GE_LT(clearIndex, 0, m_clearValues.size());
    m_clearValues[clearIndex].depthStencil = clearDepthStencil;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setSubpassCount(const uint32_t subpassCount) {
    m_inputAttachmentRefs.resize(subpassCount);
    m_colorAttachmentRefs.resize(subpassCount);
    m_resolveAttachmentRefs.resize(subpassCount);
    m_depthAttachmentRefs.resize(subpassCount);
    m_preserveAttachments.resize(subpassCount);
    m_subpasses.resize(subpassCount, VkSubpassDescription{0, VK_PIPELINE_BIND_POINT_GRAPHICS});
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setSubpassDescription(
    const uint32_t subpass, const VkPipelineBindPoint bindPoint, const VkSubpassDescriptionFlags flags) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    m_subpasses[subpass] = {flags, bindPoint};
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addInputAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const VkImageLayout imageLayout) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    m_inputAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentRefs[subpass].size());
    m_subpasses[subpass].pInputAttachments = m_inputAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addColorAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const std::optional<VkImageLayout> imageLayout) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    CRISP_CHECK_GE_LT(attachment, 0, m_attachments.size());
    m_colorAttachmentRefs[subpass].push_back(
        {attachment, imageLayout ? *imageLayout : m_attachments[attachment].finalLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addResolveAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const VkImageLayout imageLayout) {
    m_resolveAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].pResolveAttachments = m_resolveAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setDepthAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const std::optional<VkImageLayout> imageLayout) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    CRISP_CHECK_GE_LT(attachment, 0, m_attachments.size());
    m_depthAttachmentRefs[subpass] = {attachment, imageLayout ? *imageLayout : m_attachments[attachment].finalLayout};
    m_subpasses[subpass].pDepthStencilAttachment = &m_depthAttachmentRefs[subpass];
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addPreserveAttachmentRef(uint32_t subpass, uint32_t attachment) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    CRISP_CHECK_GE_LT(attachment, 0, m_attachments.size());
    m_preserveAttachments[subpass].push_back(attachment);
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

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addDependency(
    const uint32_t srcSubpass,
    const uint32_t dstSubpass,
    const VulkanSynchronizationScope& scope,
    const VkDependencyFlags flags) {
    m_dependencies.push_back(
        {srcSubpass,
         dstSubpass,
         static_cast<uint32_t>(scope.srcStage),
         static_cast<uint32_t>(scope.dstStage),
         static_cast<uint32_t>(scope.srcAccess),
         static_cast<uint32_t>(scope.dstAccess),
         flags});
    return *this;
}

std::unique_ptr<VulkanRenderPass> VulkanRenderPassBuilder::create(
    const VulkanDevice& device, const VkExtent2D& renderArea) const {

    RenderPassParameters params{};
    params.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    params.renderArea = renderArea;
    params.clearValues = m_clearValues;
    params.attachmentDescriptions = m_attachments;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
    renderPassInfo.pAttachments = m_attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    renderPassInfo.pSubpasses = m_subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(m_dependencies.size());
    renderPassInfo.pDependencies = m_dependencies.data();

    VkRenderPass renderPass{VK_NULL_HANDLE};
    VK_CHECK(vkCreateRenderPass(device.getHandle(), &renderPassInfo, nullptr, &renderPass));
    return std::make_unique<VulkanRenderPass>(device, renderPass, std::move(params));
}

} // namespace crisp