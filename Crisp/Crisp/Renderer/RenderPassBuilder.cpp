#include <Crisp/Renderer/RenderPassBuilder.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <Crisp/Enumerate.hpp>

namespace crisp
{
RenderPassBuilder::RenderPassBuilder() {}

RenderPassBuilder& RenderPassBuilder::setSwapChainDependency(bool isDependent)
{
    m_isSwapChainDependent = isDependent;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setRenderTargetsBuffered(bool renderTargetsBuffered)
{
    m_bufferedRenderTargets = renderTargetsBuffered;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAllocateRenderTagets(bool allocateRenderTargets)
{
    m_allocateRenderTargets = allocateRenderTargets;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setRenderTargetCount(uint32_t count)
{
    m_renderTargetInfos.resize(count);
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setRenderTargetFormat(
    uint32_t index, VkFormat format, VkSampleCountFlagBits sampleCount)
{
    auto& info = m_renderTargetInfos.at(index);
    info.format = format;
    info.sampleCount = sampleCount;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setRenderTargetDepthSlices(
    uint32_t index, uint32_t depthSlices, VkImageCreateFlags createFlags)
{
    auto& info = m_renderTargetInfos.at(index);
    info.depthSlices = depthSlices;
    info.createFlags = createFlags;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setRenderTargetLayersAndMipmaps(
    uint32_t index, uint32_t layerCount, uint32_t mipMapCount)
{
    auto& info = m_renderTargetInfos.at(index);
    info.layerCount = layerCount;
    info.mipmapCount = mipMapCount;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setRenderTargetBuffered(uint32_t index, bool renderTargetBuffered)
{
    auto& info = m_renderTargetInfos.at(index);
    info.buffered = renderTargetBuffered;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::configureColorRenderTarget(
    uint32_t index, VkImageUsageFlags usageFlags, VkClearColorValue clearValue)
{
    m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | usageFlags;
    m_renderTargetInfos.at(index).clearValue.color = clearValue;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::configureDepthRenderTarget(
    uint32_t index, VkImageUsageFlags usageFlags, VkClearDepthStencilValue clearValue)
{
    if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        m_renderTargetInfos.at(index).initDstStageFlags =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }
    m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | usageFlags;
    m_renderTargetInfos.at(index).clearValue.depthStencil = clearValue;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentCount(uint32_t count)
{
    m_attachments.resize(count);
    m_attachmentMappings.resize(count);
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentMapping(
    uint32_t attachmentIdx, uint32_t renderTargetIdx, uint32_t layerIdx, uint32_t layerCount)
{
    m_attachmentMappings.at(attachmentIdx).renderTargetIndex = renderTargetIdx;
    m_attachmentMappings.at(attachmentIdx).subresource.baseArrayLayer = layerIdx;
    m_attachmentMappings.at(attachmentIdx).subresource.layerCount = layerCount;
    m_attachments.at(attachmentIdx).format = m_renderTargetInfos.at(renderTargetIdx).format;
    m_attachments.at(attachmentIdx).samples = m_renderTargetInfos.at(renderTargetIdx).sampleCount;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentBufferOverDepthSlices(uint32_t attachmentIndex, bool bufferOverDepth)
{
    m_attachmentMappings.at(attachmentIndex).bufferOverDepthSlices = bufferOverDepth;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentOps(
    uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp)
{
    m_attachments[attachmentIndex].loadOp = loadOp;
    m_attachments[attachmentIndex].storeOp = storeOp;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentStencilOps(
    uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp)
{
    m_attachments[attachmentIndex].stencilLoadOp = loadOp;
    m_attachments[attachmentIndex].stencilStoreOp = storeOp;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentLayouts(
    uint32_t attachmentIndex, VkImageLayout initialLayout, VkImageLayout finalLayout)
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
    m_subpasses.resize(numSubpasses, VkSubpassDescription{0, VK_PIPELINE_BIND_POINT_GRAPHICS});
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setSubpassDescription(
    uint32_t subpass, VkPipelineBindPoint bindPoint, VkSubpassDescriptionFlags flags)
{
    m_subpasses[subpass] = {flags, bindPoint};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addInputAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout)
{
    m_inputAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentRefs[subpass].size());
    m_subpasses[subpass].pInputAttachments = m_inputAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addColorAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout)
{
    m_colorAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::addResolveAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout)
{
    m_resolveAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].pResolveAttachments = m_resolveAttachmentRefs[subpass].data();
    return *this;
}

RenderPassBuilder& RenderPassBuilder::setDepthAttachmentRef(
    uint32_t subpass, uint32_t attachment, VkImageLayout imageLayout)
{
    m_depthAttachmentRefs[subpass] = {attachment, imageLayout};
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

RenderPassBuilder& RenderPassBuilder::addDependency(
    uint32_t srcSubpass,
    uint32_t dstSubpass,
    VkPipelineStageFlags srcStageMask,
    VkAccessFlags srcAccessMask,
    VkPipelineStageFlags dstStageMask,
    VkAccessFlags dstAccessMask,
    VkDependencyFlags flags)
{
    m_dependencies.push_back({srcSubpass, dstSubpass, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, flags});
    return *this;
}

std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> RenderPassBuilder::create(VkDevice device) const
{
    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
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

std::unique_ptr<VulkanRenderPass> RenderPassBuilder::create(const VulkanDevice& device, VkExtent2D renderArea) const
{
    auto [handle, attachments] = create(device.getHandle());

    RenderPassDescription description{};
    description.bufferedRenderTargets = m_bufferedRenderTargets;
    description.isSwapChainDependent = m_isSwapChainDependent;
    description.allocateRenderTargets = m_allocateRenderTargets;
    description.renderArea = renderArea;
    description.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    description.attachmentDescriptions = std::move(attachments);
    description.attachmentMappings = m_attachmentMappings;
    description.renderTargetInfos = m_renderTargetInfos;

    return std::make_unique<VulkanRenderPass>(device, handle, std::move(description));
}

} // namespace crisp