#include "VulkanRenderPass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanEnumToString.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <CrispCore/Enumerate.hpp>

namespace crisp
{

VulkanRenderPass::VulkanRenderPass(const VulkanDevice& device, bool isSwapChainDependent, uint32_t subpassCount)
    : VulkanResource(nullptr, device.getResourceDeallocator())
    , m_isWindowSizeDependent(isSwapChainDependent)
    , m_numSubpasses(subpassCount)
    , m_defaultSampleCount(VK_SAMPLE_COUNT_1_BIT)
{
}

VulkanRenderPass::VulkanRenderPass(const VulkanDevice& device, VkRenderPass renderPass,
    RenderPassDescription&& description)
    : VulkanResource(renderPass, device.getResourceDeallocator())
    , m_isWindowSizeDependent(description.isSwapChainDependent)
    , m_renderArea(description.renderArea)
    , m_numSubpasses(description.subpassCount)
    , m_defaultSampleCount(VK_SAMPLE_COUNT_1_BIT)
    , m_attachmentDescriptions(std::move(description.attachmentDescriptions))
    , m_attachmentMappings(std::move(description.attachmentMappings))
    , m_renderTargetInfos(std::move(description.renderTargetInfos))
    , m_bufferedRenderTargets(description.bufferedRenderTargets)
    , m_allocateRenderTargets(description.allocateRenderTargets)
{
    if (description.renderArea.width == 0 || description.renderArea.height == 0)
    {
        spdlog::critical("Render area of zero specified: [{}, {}]", description.renderArea.width,
            description.renderArea.height);
        std::exit(-1);
    }

    m_attachmentClearValues.resize(m_attachmentDescriptions.size());
    for (const auto& [i, clearValue] : enumerate(m_attachmentClearValues))
    {
        const auto& mapping = m_attachmentMappings.at(i);
        clearValue = m_renderTargetInfos.at(mapping.renderTargetIndex).clearValue;
    }

    createResources(device);
}

VulkanRenderPass::~VulkanRenderPass()
{
    freeResources();
}

void VulkanRenderPass::recreate(const VulkanDevice& device, const VkExtent2D& swapChainExtent)
{
    if (m_isWindowSizeDependent)
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
    return { 0.0f, 0.0f, static_cast<float>(m_renderArea.width), static_cast<float>(m_renderArea.height), 0.0f, 1.0f };
}

VkRect2D VulkanRenderPass::createScissor() const
{
    return {
        {0, 0},
        m_renderArea
    };
}

void VulkanRenderPass::begin(const VkCommandBuffer cmdBuffer, const uint32_t frameIndex,
    const VkSubpassContents content) const
{
    VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = m_framebuffers.at(frameIndex)->getHandle();
    renderPassInfo.renderArea.offset = { 0, 0 };
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

void VulkanRenderPass::nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents contents) const
{
    vkCmdNextSubpass(cmdBuffer, contents);
}

VulkanImage& VulkanRenderPass::getRenderTarget(const uint32_t index) const
{
    return *m_renderTargets.at(index);
}

const VulkanImageView& VulkanRenderPass::getRenderTargetView(const uint32_t index, const uint32_t frameIndex) const
{
    return *m_renderTargetViews.at(frameIndex).at(index);
}

std::vector<VulkanImageView*> VulkanRenderPass::getRenderTargetViews(uint32_t renderTargetIndex) const
{
    std::vector<VulkanImageView*> imageViews(m_renderTargetViews.size());
    for (uint32_t i = 0; i < m_renderTargetViews.size(); ++i)
        imageViews.at(i) = m_renderTargetViews.at(i).at(renderTargetIndex).get();
    return imageViews;
}

std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(const VulkanDevice& device, uint32_t index,
    uint32_t numFrames) const
{
    return std::make_unique<VulkanImageView>(device, *m_renderTargets.at(index),
        numFrames > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, 0, numFrames, 0, 1);
}

std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(const VulkanDevice& device, uint32_t index,
    uint32_t baseLayer, uint32_t numLayers) const
{
    return std::make_unique<VulkanImageView>(device, *m_renderTargets.at(index),
        numLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, baseLayer, numLayers);
}

std::unique_ptr<VulkanImage> VulkanRenderPass::extractRenderTarget(uint32_t index)
{
    std::unique_ptr<VulkanImage> image = std::move(m_renderTargets.at(index));
    m_renderTargets.erase(m_renderTargets.begin() + index);
    return image;
}

VkSampleCountFlagBits VulkanRenderPass::getDefaultSampleCount() const
{
    return m_defaultSampleCount;
}

void VulkanRenderPass::updateInitialLayouts(VkCommandBuffer cmdBuffer)
{
    for (const auto& [i, renderTarget] : enumerate(m_renderTargets))
    {
        const auto& desc = m_attachmentDescriptions.at(i);
        const auto& info = m_renderTargetInfos.at(i);
        renderTarget->transitionLayout(cmdBuffer, desc.initialLayout, info.initSrcStageFlags, info.initDstStageFlags);
    }
}

void VulkanRenderPass::createRenderTargets(const VulkanDevice& device)
{
    m_renderTargets.resize(m_renderTargetInfos.size());
    for (const auto& [i, info] : enumerate(m_renderTargetInfos))
    {
        auto layerMultiplier = m_bufferedRenderTargets ? RendererConfig::VirtualFrameCount : 1;
        if (info.buffered.has_value())
        {
            layerMultiplier = *info.buffered ? RendererConfig::VirtualFrameCount : 1;
        }

        const VkExtent3D dims = { m_renderArea.width, m_renderArea.height, info.depthSlices };
        const uint32_t totalLayerCount = info.layerCount * layerMultiplier;
        m_renderTargets[i] = std::make_unique<VulkanImage>(device, dims, totalLayerCount, info.mipmapCount, info.format,
            info.usage, determineImageAspect(info.format), info.createFlags);
    }
}

void VulkanRenderPass::createRenderTargetViewsAndFramebuffers(const VulkanDevice& device)
{
    // Create layerCount x attachmentCount render target views
    const auto layerMultiplier = m_bufferedRenderTargets ? RendererConfig::VirtualFrameCount : 1;
    m_renderTargetViews.resize(layerMultiplier);
    m_framebuffers.resize(layerMultiplier);

    const size_t renderTargetViewCount = m_attachmentDescriptions.size();
    for (const auto& [layerIdx, rtViews] : enumerate(m_renderTargetViews))
    {
        rtViews.resize(renderTargetViewCount);
        std::vector<VkImageView> rtViewHandles(renderTargetViewCount);
        for (const auto& [attachmentIdx, mapping] : enumerate(m_attachmentMappings))
        {
            auto& renderTarget = *m_renderTargets.at(mapping.renderTargetIndex);
            auto& renderTargetInfo = m_renderTargetInfos.at(mapping.renderTargetIndex);

            if (mapping.bufferOverDepthSlices)
            {
                const uint32_t depthSliceCount = renderTargetInfo.depthSlices / layerMultiplier;
                const uint32_t baseDepthOffset = static_cast<uint32_t>(layerIdx) * depthSliceCount;
                const VkImageViewType type = depthSliceCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                rtViews.at(attachmentIdx) = renderTarget.createView(type, baseDepthOffset, depthSliceCount);
                rtViewHandles.at(attachmentIdx) = rtViews.at(attachmentIdx)->getHandle();
            }
            else
            {
                const uint32_t baseLayerOffset = static_cast<uint32_t>(layerIdx) * renderTargetInfo.layerCount;
                const uint32_t firstLayer = baseLayerOffset + mapping.subresource.baseArrayLayer;
                const uint32_t layerCount = mapping.subresource.layerCount;
                const VkImageViewType type = layerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                rtViews.at(attachmentIdx) = renderTarget.createView(type, firstLayer, layerCount);
                rtViewHandles.at(attachmentIdx) = rtViews.at(attachmentIdx)->getHandle();
            }
        }

        m_framebuffers[layerIdx] = std::make_unique<VulkanFramebuffer>(device, m_handle, m_renderArea, rtViewHandles);
    }
}

VkExtent3D VulkanRenderPass::getRenderAreaExtent() const
{
    return { m_renderArea.width, m_renderArea.height, 1u };
}

void VulkanRenderPass::createResources(const VulkanDevice& device)
{
    if (m_allocateRenderTargets)
    {
        createRenderTargets(device);
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

void VulkanRenderPass::setDepthRenderTargetInfo(uint32_t index, VkImageUsageFlags additionalFlags,
    VkClearDepthStencilValue clearValue)
{
    m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalFlags;

    if (m_attachmentClearValues.size() <= index)
    {
        m_attachmentClearValues.resize(index + 1);
    }
    m_attachmentClearValues.at(index).depthStencil = clearValue;
}

void VulkanRenderPass::setColorRenderTargetInfo(uint32_t index, VkImageUsageFlags additionalFlags,
    VkClearColorValue clearValue)
{
    m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | additionalFlags;

    if (m_attachmentClearValues.size() <= index)
    {
        m_attachmentClearValues.resize(index + 1);
    }
    m_attachmentClearValues.at(index).color = clearValue;
}
} // namespace crisp
