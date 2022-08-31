#include "VulkanRenderPass.hpp"

#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanEnumToString.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <Crisp/Enumerate.hpp>

namespace crisp
{

// VulkanRenderPass::VulkanRenderPass(
//     const VulkanDevice& device,
//     std::vector<std::shared_ptr<VulkanImage>>&& renderTargets,
//     bool isSwapChainDependent,
//     uint32_t subpassCount)
//     : VulkanResource(nullptr, device.getResourceDeallocator())
//     , m_isWindowSizeDependent(isSwapChainDependent)
//     , m_numSubpasses(subpassCount)
//     , m_defaultSampleCount(VK_SAMPLE_COUNT_1_BIT)
//     , m_renderTargets(std::move(renderTargets))
//{
// }

VulkanRenderPass::VulkanRenderPass(
    const VulkanDevice& device, const VkRenderPass renderPass, RenderPassDescription&& description)
    : VulkanResource(renderPass, device.getResourceDeallocator())
    , m_isSwapChainDependent(description.isSwapChainDependent)
    , m_renderArea(description.renderArea)
    , m_subpassCount(description.subpassCount)
    , m_defaultSampleCount(VK_SAMPLE_COUNT_1_BIT)
    , m_renderTargets(std::move(description.renderTargets))
    , m_attachmentDescriptions(std::move(description.attachmentDescriptions))
    , m_attachmentMappings(std::move(description.attachmentMappings))
    , m_allocateResources(description.allocateRenderTargets)
{
    if (m_renderArea.width == 0 || m_renderArea.height == 0)
    {
        spdlog::critical("Render area of zero specified: [{}, {}]", m_renderArea.width, m_renderArea.height);
        std::exit(-1);
    }

    m_attachmentClearValues.resize(m_attachmentDescriptions.size());
    for (const auto& [i, clearValue] : enumerate(m_attachmentClearValues))
    {
        const auto& mapping = m_attachmentMappings.at(i);
        clearValue = m_renderTargets.at(mapping.renderTargetIndex)->info.clearValue;
    }

    createResources(device);
}

VulkanRenderPass::~VulkanRenderPass()
{
    freeResources();
}

void VulkanRenderPass::recreate(const VulkanDevice& device, const VkExtent2D& swapChainExtent)
{
    if (m_isSwapChainDependent)
    {
        freeResources();

        m_renderArea = swapChainExtent;
        createResources(device);
    }
}

VkExtent2D VulkanRenderPass::getRenderArea() const
{
    return m_renderArea;
}

VkViewport VulkanRenderPass::createViewport() const
{
    return {0.0f, 0.0f, static_cast<float>(m_renderArea.width), static_cast<float>(m_renderArea.height), 0.0f, 1.0f};
}

VkRect2D VulkanRenderPass::createScissor() const
{
    constexpr VkOffset2D zeroOrigin{0, 0};
    return {zeroOrigin, m_renderArea};
}

void VulkanRenderPass::begin(
    const VkCommandBuffer cmdBuffer, const uint32_t frameIndex, const VkSubpassContents content) const
{
    VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = m_framebuffers.at(frameIndex)->getHandle();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_renderArea;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(m_attachmentClearValues.size());
    renderPassInfo.pClearValues = m_attachmentClearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, content);
}

void VulkanRenderPass::end(const VkCommandBuffer cmdBuffer, const uint32_t frameIndex)
{
    vkCmdEndRenderPass(cmdBuffer);

    if (m_renderTargetViews.empty())
    {
        return;
    }

    for (const auto& [i, renderTargetView] : enumerate(m_renderTargetViews.at(frameIndex)))
    {
        auto& image = renderTargetView->getImage();
        image.setImageLayout(m_attachmentDescriptions.at(i).finalLayout, renderTargetView->getSubresourceRange());
    }
}

void VulkanRenderPass::nextSubpass(const VkCommandBuffer cmdBuffer, const VkSubpassContents contents) const
{
    vkCmdNextSubpass(cmdBuffer, contents);
}

VulkanImage& VulkanRenderPass::getRenderTarget(const uint32_t index) const
{
    return *m_renderTargets.at(index)->image;
}

const VulkanImageView& VulkanRenderPass::getRenderTargetView(
    const uint32_t attachmentIndex, const uint32_t frameIndex) const
{
    return *m_renderTargetViews.at(frameIndex).at(attachmentIndex);
}

std::vector<VulkanImageView*> VulkanRenderPass::getRenderTargetViews(uint32_t renderTargetIndex) const
{
    std::vector<VulkanImageView*> imageViews(m_renderTargetViews.size());
    for (uint32_t i = 0; i < m_renderTargetViews.size(); ++i)
        imageViews.at(i) = m_renderTargetViews.at(i).at(renderTargetIndex).get();
    return imageViews;
}

std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(
    const VulkanDevice& device, const uint32_t attachmentIndex, const uint32_t frameCount) const
{
    return std::make_unique<VulkanImageView>(
        device,
        *m_renderTargets.at(attachmentIndex)->image,
        frameCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
        0,
        frameCount,
        0,
        1);
}

std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(
    const VulkanDevice& device,
    const uint32_t attachmentIndex,
    const uint32_t baseLayer,
    const uint32_t layerCount) const
{
    return std::make_unique<VulkanImageView>(
        device,
        *m_renderTargets.at(attachmentIndex)->image,
        layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
        baseLayer,
        layerCount);
}

void VulkanRenderPass::updateInitialLayouts(VkCommandBuffer cmdBuffer)
{
    for (const auto& frameRenderTargetViews : m_renderTargetViews)
    {
        for (const auto& [i, renderTargetView] : enumerate(frameRenderTargetViews))
        {
            auto& image = renderTargetView->getImage();
            auto& desc = m_attachmentDescriptions.at(i);
            auto& mapping = m_attachmentMappings.at(i);
            auto& info = m_renderTargets.at(mapping.renderTargetIndex)->info;
            // image.setImageLayout(desc.finalLayout, renderTargetView->getSubresourceRange());
            image.transitionLayout(
                cmdBuffer,
                desc.initialLayout,
                renderTargetView->getSubresourceRange(),
                info.initSrcStageFlags,
                info.initDstStageFlags);
        }
    }

    /*for (const auto& [i, renderTarget] : enumerate(m_renderTargets))
    {
        const auto& desc = m_attachmentDescriptions.at(i);
        const auto& info = renderTarget->info;
        renderTarget->image->transitionLayout(
            cmdBuffer, desc.initialLayout, info.initSrcStageFlags, info.initDstStageFlags);
    }*/
}

// void VulkanRenderPass::createRenderTargets(const VulkanDevice& device)
//{
//     m_renderTargets.resize(m_renderTargetInfos.size());
//     for (const auto& [i, info] : enumerate(m_renderTargetInfos))
//     {
//         auto layerMultiplier = m_bufferedRenderTargets ? RendererConfig::VirtualFrameCount : 1;
//         if (info.buffered.has_value())
//         {
//             layerMultiplier = *info.buffered ? RendererConfig::VirtualFrameCount : 1;
//         }
//
//         const VkExtent3D dims = {m_renderArea.width, m_renderArea.height, info.depthSlices};
//         const uint32_t totalLayerCount = info.layerCount * layerMultiplier;
//         m_renderTargets[i] = std::make_unique<VulkanImage>(
//             device,
//             dims,
//             totalLayerCount,
//             info.mipmapCount,
//             info.format,
//             info.usage,
//             determineImageAspect(info.format),
//             info.createFlags);
//     }
// }

void VulkanRenderPass::createRenderTargetViewsAndFramebuffers(const VulkanDevice& device)
{
    // Check if any render target is buffered. In that case, we'll buffer the views.
    uint32_t layerMultiplier{1};
    for (const auto& rt : m_renderTargets)
    {
        if (rt->info.buffered)
        {
            layerMultiplier = RendererConfig::VirtualFrameCount;
            break;
        }
    }
    m_renderTargetViews.resize(layerMultiplier);
    m_framebuffers.resize(layerMultiplier);

    const size_t renderTargetViewCount{m_attachmentDescriptions.size()};
    for (const auto& [layerIdx, rtViews] : enumerate(m_renderTargetViews))
    {
        uint32_t framebufferLayers = 1;
        rtViews.resize(renderTargetViewCount);
        std::vector<VkImageView> attachmentViewHandles(renderTargetViewCount);
        for (const auto& [attachmentIdx, mapping] : enumerate(m_attachmentMappings))
        {
            auto& renderTarget = *m_renderTargets.at(mapping.renderTargetIndex);

            if (mapping.bufferOverDepthSlices)
            {
                const uint32_t depthSliceCount = renderTarget.info.depthSlices / layerMultiplier;
                const uint32_t baseDepthOffset = static_cast<uint32_t>(layerIdx) * depthSliceCount;
                const VkImageViewType type = depthSliceCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                rtViews.at(attachmentIdx) = renderTarget.image->createView(type, baseDepthOffset, depthSliceCount);
                attachmentViewHandles.at(attachmentIdx) = rtViews.at(attachmentIdx)->getHandle();
                framebufferLayers = depthSliceCount;
            }
            else
            {
                const uint32_t baseLayerOffset = static_cast<uint32_t>(layerIdx) * renderTarget.info.layerCount;
                const uint32_t firstLayer = baseLayerOffset + mapping.subresource.baseArrayLayer;
                const uint32_t layerCount = mapping.subresource.layerCount;
                const VkImageViewType type = layerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                rtViews.at(attachmentIdx) = renderTarget.image->createView(
                    type, firstLayer, layerCount, mapping.subresource.baseMipLevel, mapping.subresource.levelCount);
                attachmentViewHandles.at(attachmentIdx) = rtViews.at(attachmentIdx)->getHandle();
            }
        }

        m_framebuffers[layerIdx] = std::make_unique<VulkanFramebuffer>(
            device, m_handle, m_renderArea, attachmentViewHandles, framebufferLayers);
    }
}

VkExtent3D VulkanRenderPass::getRenderAreaExtent() const
{
    return {m_renderArea.width, m_renderArea.height, 1u};
}

void VulkanRenderPass::createResources(const VulkanDevice& device)
{
    if (m_allocateResources)
    {
        // createRenderTargets(device);
        createRenderTargetViewsAndFramebuffers(device);
    }
    else
    {
        m_framebuffers.resize(RendererConfig::VirtualFrameCount);
    }
}

void VulkanRenderPass::freeResources()
{
    m_renderTargets.clear();
    m_renderTargetViews.clear();
    m_framebuffers.clear();
}

// void VulkanRenderPass::setDepthRenderTargetInfo(
//     uint32_t index, VkImageUsageFlags additionalFlags, VkClearDepthStencilValue clearValue)
//{
//     m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
//     m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalFlags;
//
//     if (m_attachmentClearValues.size() <= index)
//     {
//         m_attachmentClearValues.resize(index + 1);
//     }
//     m_attachmentClearValues.at(index).depthStencil = clearValue;
// }
//
// void VulkanRenderPass::setColorRenderTargetInfo(
//     uint32_t index, VkImageUsageFlags additionalFlags, VkClearColorValue clearValue)
//{
//     m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//     m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | additionalFlags;
//
//     if (m_attachmentClearValues.size() <= index)
//     {
//         m_attachmentClearValues.resize(index + 1);
//     }
//     m_attachmentClearValues.at(index).color = clearValue;
// }
} // namespace crisp
