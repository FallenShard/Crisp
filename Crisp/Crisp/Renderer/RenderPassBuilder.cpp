#include <Crisp/Renderer/RenderPassBuilder.hpp>

#include <Crisp/Vulkan/VulkanChecks.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <Crisp/Core/Checks.hpp>

#include <ranges>

namespace crisp {
namespace {
bool checkSwapChainDependency(const std::vector<RenderTarget*>& renderTargets) {
    return std::ranges::any_of(renderTargets, [](const auto& rt) { return rt->info.isSwapChainDependent; });
}

} // namespace

RenderPassBuilder& RenderPassBuilder::setAllocateAttachmentViews(bool allocateAttachmentViews) {
    m_allocateAttachmentViews = allocateAttachmentViews;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentCount(uint32_t count) {
    m_attachments.resize(count);
    m_attachmentMappings.resize(count);
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentMapping(
    const uint32_t attachmentIdx,
    const uint32_t renderTargetIdx,
    const uint32_t firstLayer,
    const uint32_t layerCount,
    const uint32_t firstMipLevel,
    const uint32_t mipLevelCount) {
    m_attachmentMappings.at(attachmentIdx).renderTargetIndex = renderTargetIdx;
    m_attachmentMappings.at(attachmentIdx).subresource.baseArrayLayer = firstLayer;
    m_attachmentMappings.at(attachmentIdx).subresource.layerCount = layerCount;
    m_attachmentMappings.at(attachmentIdx).subresource.baseMipLevel = firstMipLevel;
    m_attachmentMappings.at(attachmentIdx).subresource.levelCount = mipLevelCount;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentBufferOverDepthSlices(
    uint32_t attachmentIndex, bool bufferOverDepth) {
    m_attachmentMappings.at(attachmentIndex).bufferOverDepthSlices = bufferOverDepth;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentOps(
    uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp) {
    m_attachments.at(attachmentIndex).loadOp = loadOp;
    m_attachments.at(attachmentIndex).storeOp = storeOp;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentStencilOps(
    uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp) {
    m_attachments.at(attachmentIndex).stencilLoadOp = loadOp;
    m_attachments.at(attachmentIndex).stencilStoreOp = storeOp;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentLayouts(
    uint32_t attachmentIndex, VkImageLayout initialLayout, VkImageLayout finalLayout) {
    m_attachments.at(attachmentIndex).initialLayout = initialLayout;
    m_attachments.at(attachmentIndex).finalLayout = finalLayout;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setNumSubpasses(uint32_t numSubpasses) {
    m_inputAttachmentRefs.resize(numSubpasses);
    m_colorAttachmentRefs.resize(numSubpasses);
    m_resolveAttachmentRefs.resize(numSubpasses);
    m_depthAttachmentRefs.resize(numSubpasses);
    m_preserveAttachments.resize(numSubpasses);
    m_subpasses.resize(numSubpasses, VkSubpassDescription{0, VK_PIPELINE_BIND_POINT_GRAPHICS});
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setSubpassDescription(
    uint32_t subpass, VkPipelineBindPoint bindPoint, VkSubpassDescriptionFlags flags) {
    m_subpasses.at(subpass) = {flags, bindPoint};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addInputAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout) {
    m_inputAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentRefs[subpass].size());
    m_subpasses[subpass].pInputAttachments = m_inputAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addColorAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout) {
    m_colorAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addResolveAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout) {
    m_resolveAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].pResolveAttachments = m_resolveAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setDepthAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout) {
    m_depthAttachmentRefs[subpass] = {attachment, imageLayout};
    m_subpasses[subpass].pDepthStencilAttachment = &m_depthAttachmentRefs[subpass];
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addPreserveAttachmentRef(uint32_t subpass, uint32_t attachment) {
    m_preserveAttachments[subpass].push_back(attachment);
    m_subpasses[subpass].preserveAttachmentCount = static_cast<uint32_t>(m_preserveAttachments[subpass].size());
    m_subpasses[subpass].pPreserveAttachments = m_preserveAttachments[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addDependency(
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

std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> RenderPassBuilder::create(
    VkDevice device, std::vector<RenderTarget*> renderTargets) const {
    std::vector<VkAttachmentDescription> attachments(m_attachments);
    for (const auto& [i, attachment] : std::views::enumerate(attachments)) {
        attachment.format = renderTargets.at(m_attachmentMappings[i].renderTargetIndex)->info.format;
        attachment.samples = renderTargets.at(m_attachmentMappings[i].renderTargetIndex)->info.sampleCount;
    }

    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    renderPassInfo.pSubpasses = m_subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(m_dependencies.size());
    renderPassInfo.pDependencies = m_dependencies.data();

    VkRenderPass renderPass{VK_NULL_HANDLE};
    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
    return std::make_pair(renderPass, std::move(attachments));
}

std::unique_ptr<VulkanRenderPass> RenderPassBuilder::create(
    const VulkanDevice& device, VkExtent2D renderArea, const std::vector<RenderTarget*>& renderTargets) const {
    auto [handle, attachments] = create(device.getHandle(), renderTargets);

    RenderPassParameters params{};
    params.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    params.renderArea = renderArea;
    params.isSwapChainDependent = checkSwapChainDependency(renderTargets);

    params.allocateAttachmentViews = m_allocateAttachmentViews;
    params.attachmentDescriptions = std::move(attachments);
    params.attachmentMappings = m_attachmentMappings;

    for (const auto& rt : renderTargets) {
        params.renderTargetInfos.push_back(rt->info);
        params.renderTargets.push_back(rt->image.get());
    }

    return std::make_unique<VulkanRenderPass>(device, handle, std::move(params));
}

} // namespace crisp