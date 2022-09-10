#include "VulkanRenderPass.hpp"

#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanEnumToString.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <Crisp/Common/Checks.hpp>
#include <Crisp/Enumerate.hpp>

namespace crisp
{

VulkanRenderPass::VulkanRenderPass(
    const VulkanDevice& device, const VkRenderPass renderPass, RenderPassDescription&& description)
    : VulkanResource(renderPass, device.getResourceDeallocator())
    , m_isSwapChainDependent(description.isSwapChainDependent)
    , m_allocateAttachmentViews(description.allocateAttachmentViews)
    , m_renderArea(description.renderArea)
    , m_subpassCount(description.subpassCount)
    , m_renderTargets(std::move(description.renderTargets))
    , m_attachmentDescriptions(std::move(description.attachmentDescriptions))
    , m_attachmentMappings(std::move(description.attachmentMappings))
{
    CRISP_CHECK(m_renderArea.width > 0, "Render area has non-positive width!");
    CRISP_CHECK(m_renderArea.height > 0, "Render area has non-positive height!");
    CRISP_CHECK(
        m_attachmentMappings.size() == m_attachmentDescriptions.size(), "Attachment and description size mismatch!");

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

    if (m_attachmentViews.empty())
        return;

    for (const auto& [i, attachmentView] : enumerate(m_attachmentViews.at(frameIndex)))
    {
        const uint32_t renderTargetIndex{m_attachmentMappings.at(i).renderTargetIndex};
        auto& image = *m_renderTargets.at(renderTargetIndex)->image;

        // We unconditionally set the layout here because the render pass did an automatic layout transition.
        image.setImageLayout(m_attachmentDescriptions.at(i).finalLayout, attachmentView->getSubresourceRange());
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
    return *m_attachmentViews.at(frameIndex).at(attachmentIndex);
}

std::vector<VulkanImageView*> VulkanRenderPass::getRenderTargetViews(uint32_t renderTargetIndex) const
{
    std::vector<VulkanImageView*> perFrameAttachmentViews(m_attachmentViews.size());
    for (uint32_t i = 0; i < perFrameAttachmentViews.size(); ++i)
        perFrameAttachmentViews[i] = m_attachmentViews[i].at(renderTargetIndex).get();
    return perFrameAttachmentViews;
}

void VulkanRenderPass::updateInitialLayouts(VkCommandBuffer cmdBuffer)
{
    for (const auto& frameAttachmentViews : m_attachmentViews)
    {
        for (const auto& [i, attachmentView] : enumerate(frameAttachmentViews))
        {
            const auto renderTargetIndex{m_attachmentMappings.at(i).renderTargetIndex};
            const auto initialLayout{m_attachmentDescriptions.at(i).initialLayout};
            auto& info = m_renderTargets.at(renderTargetIndex)->info;
            auto& image = *m_renderTargets.at(renderTargetIndex)->image;

            image.transitionLayout(
                cmdBuffer,
                initialLayout,
                attachmentView->getSubresourceRange(),
                info.initSrcStageFlags,
                info.initDstStageFlags);
        }
    }
}

void VulkanRenderPass::createRenderTargetViewsAndFramebuffers(const VulkanDevice& device)
{
    m_attachmentViews.resize(RendererConfig::VirtualFrameCount);
    for (const auto& [frameIdx, frameAttachmentViews] : enumerate(m_attachmentViews))
    {
        // Typically 1, but for depth slice rendering, we need to modify it.
        uint32_t framebufferLayerCount{1};
        frameAttachmentViews.resize(m_attachmentDescriptions.size());
        std::vector<VkImageView> attachmentViewHandles(frameAttachmentViews.size());
        for (const auto& [attachmentIdx, mapping] : enumerate(m_attachmentMappings))
        {
            auto& renderTarget = *m_renderTargets.at(mapping.renderTargetIndex);
            if (mapping.bufferOverDepthSlices)
            {
                // TODO: fix this to have similar interface to layer-based buffering.
                framebufferLayerCount = renderTarget.info.buffered
                                            ? renderTarget.info.depthSlices / RendererConfig::VirtualFrameCount
                                            : renderTarget.info.depthSlices;
                const uint32_t frameDepthOffset =
                    renderTarget.info.buffered ? static_cast<uint32_t>(frameIdx) * framebufferLayerCount : 0;
                const VkImageViewType type =
                    framebufferLayerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                frameAttachmentViews[attachmentIdx] =
                    renderTarget.image->createView(type, frameDepthOffset, framebufferLayerCount);
            }
            else
            {
                // This handles the case where the render target isn't buffered. In that case, we create another
                // image view that essentially points to the same region as the virtual frame 0.
                const uint32_t frameLayerOffset =
                    renderTarget.info.buffered ? static_cast<uint32_t>(frameIdx) * renderTarget.info.layerCount : 0;
                const uint32_t firstLayer = frameLayerOffset + mapping.subresource.baseArrayLayer;
                const uint32_t layerCount = mapping.subresource.layerCount;
                const VkImageViewType type = layerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                frameAttachmentViews[attachmentIdx] = renderTarget.image->createView(
                    type, firstLayer, layerCount, mapping.subresource.baseMipLevel, mapping.subresource.levelCount);
            }

            attachmentViewHandles[attachmentIdx] = frameAttachmentViews[attachmentIdx]->getHandle();
        }

        m_framebuffers[frameIdx] = std::make_unique<VulkanFramebuffer>(
            device, m_handle, m_renderArea, attachmentViewHandles, framebufferLayerCount);
    }
}

VkExtent3D VulkanRenderPass::getRenderAreaExtent() const
{
    return {m_renderArea.width, m_renderArea.height, 1u};
}

void VulkanRenderPass::createResources(const VulkanDevice& device)
{
    m_framebuffers.resize(RendererConfig::VirtualFrameCount);
    if (m_allocateAttachmentViews)
    {
        createRenderTargetViewsAndFramebuffers(device);
    }
}

void VulkanRenderPass::freeResources()
{
    m_attachmentViews.clear();
    m_framebuffers.clear();
}

} // namespace crisp
