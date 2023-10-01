#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <Crisp/Core/Checks.hpp>

#include <ranges>

namespace crisp {

VulkanRenderPass::VulkanRenderPass(
    const VulkanDevice& device, const VkRenderPass handle, RenderPassParameters&& parameters)
    : VulkanResource(handle, device.getResourceDeallocator())
    , m_params(std::move(parameters)) {
    CRISP_CHECK(m_params.renderArea.width > 0, "Render area has non-positive width!");
    CRISP_CHECK(m_params.renderArea.height > 0, "Render area has non-positive height!");
    CRISP_CHECK(
        m_params.attachmentMappings.size() == m_params.attachmentDescriptions.size(),
        "Attachment mapping and description size mismatch!");

    m_attachmentClearValues.resize(m_params.attachmentDescriptions.size());
    for (const auto& [i, clearValue] : std::views::enumerate(m_attachmentClearValues)) {
        const auto& mapping = m_params.attachmentMappings.at(i);
        clearValue = m_params.renderTargetInfos.at(mapping.renderTargetIndex).clearValue;
    }

    createResources(device);
}

VulkanRenderPass::VulkanRenderPass(
    const VulkanDevice& device,
    const VkRenderPass handle,
    RenderPassParameters&& parameters,
    std::vector<VkClearValue>&& clearValues)
    : VulkanResource(handle, device.getResourceDeallocator())
    , m_params(std::move(parameters))
    , m_attachmentClearValues(std::move(clearValues)) {
    CRISP_CHECK(m_params.renderArea.width > 0, "Render area has non-positive width!");
    CRISP_CHECK(m_params.renderArea.height > 0, "Render area has non-positive height!");
    CRISP_CHECK_EQ(
        m_params.attachmentMappings.size(),
        m_params.attachmentDescriptions.size(),
        "Attachment mapping and description size mismatch!");

    createResources(device);
}

void VulkanRenderPass::recreate(const VulkanDevice& device, const VkExtent2D& swapChainExtent) {
    if (m_params.isSwapChainDependent) {
        freeResources();

        m_params.renderArea = swapChainExtent;
        createResources(device);
    }
}

VkExtent2D VulkanRenderPass::getRenderArea() const {
    return m_params.renderArea;
}

VkViewport VulkanRenderPass::createViewport() const {
    return {
        0.0f,
        0.0f,
        static_cast<float>(m_params.renderArea.width),
        static_cast<float>(m_params.renderArea.height),
        0.0f,
        1.0f};
}

VkRect2D VulkanRenderPass::createScissor() const {
    constexpr VkOffset2D zeroOrigin{0, 0};
    return {zeroOrigin, m_params.renderArea};
}

void VulkanRenderPass::begin(
    const VkCommandBuffer cmdBuffer, const uint32_t frameIndex, const VkSubpassContents content) const {
    VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = m_framebuffers.at(frameIndex)->getHandle();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_params.renderArea;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(m_attachmentClearValues.size());
    renderPassInfo.pClearValues = m_attachmentClearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, content);
}

void VulkanRenderPass::end(const VkCommandBuffer cmdBuffer, const uint32_t frameIndex) {
    vkCmdEndRenderPass(cmdBuffer);

    if (m_attachmentViews.empty()) {
        return;
    }

    for (const auto& [i, attachmentView] : std::views::enumerate(m_attachmentViews.at(frameIndex))) {
        const uint32_t renderTargetIndex{m_params.attachmentMappings.at(i).renderTargetIndex};
        auto& image = *m_params.renderTargets.at(renderTargetIndex);

        // We unconditionally set the layout here because the render pass did an automatic layout transition.
        image.setImageLayout(m_params.attachmentDescriptions.at(i).finalLayout, attachmentView->getSubresourceRange());
    }
}

void VulkanRenderPass::nextSubpass(const VkCommandBuffer cmdBuffer, const VkSubpassContents contents) const {
    vkCmdNextSubpass(cmdBuffer, contents);
}

VulkanImage& VulkanRenderPass::getRenderTarget(const uint32_t index) const {
    return *m_params.renderTargets.at(index);
}

VulkanImage& VulkanRenderPass::getRenderTargetFromAttachmentIndex(const uint32_t attachmentIndex) const {
    const uint32_t renderTargetIndex{m_params.attachmentMappings.at(attachmentIndex).renderTargetIndex};
    return *m_params.renderTargets.at(renderTargetIndex);
}

const VulkanImageView& VulkanRenderPass::getAttachmentView(
    const uint32_t attachmentIndex, const uint32_t frameIndex) const {
    return *m_attachmentViews.at(frameIndex).at(attachmentIndex);
}

std::vector<VulkanImageView*> VulkanRenderPass::getAttachmentViews(uint32_t renderTargetIndex) const {
    std::vector<VulkanImageView*> perFrameAttachmentViews(m_attachmentViews.size());
    for (uint32_t i = 0; i < perFrameAttachmentViews.size(); ++i) {
        perFrameAttachmentViews[i] = m_attachmentViews[i].at(renderTargetIndex).get();
    }
    return perFrameAttachmentViews;
}

void VulkanRenderPass::updateInitialLayouts(VkCommandBuffer cmdBuffer) {
    for (const auto& frameAttachmentViews : m_attachmentViews) {
        for (const auto& [i, attachmentView] : std::views::enumerate(frameAttachmentViews)) {
            const auto renderTargetIndex{m_params.attachmentMappings.at(i).renderTargetIndex};
            const auto initialLayout{m_params.attachmentDescriptions.at(i).initialLayout};
            auto& info = m_params.renderTargetInfos.at(renderTargetIndex);
            auto& image = *m_params.renderTargets.at(renderTargetIndex);

            image.transitionLayout(
                cmdBuffer,
                initialLayout,
                attachmentView->getSubresourceRange(),
                info.initSrcStageFlags,
                info.initDstStageFlags);
        }
    }
}

void VulkanRenderPass::createRenderTargetViewsAndFramebuffers(const VulkanDevice& device) {
    m_attachmentViews.resize(RendererConfig::VirtualFrameCount);
    for (const auto& [frameIdx, frameAttachmentViews] : std::views::enumerate(m_attachmentViews)) {
        // Typically 1, but for depth slice rendering, we need to modify it.
        uint32_t framebufferLayerCount{1};
        frameAttachmentViews.resize(m_params.attachmentDescriptions.size());
        std::vector<VkImageView> attachmentViewHandles(frameAttachmentViews.size());
        for (const auto& [attachmentIdx, mapping] : std::views::enumerate(m_params.attachmentMappings)) {
            const auto& renderTargetInfo = m_params.renderTargetInfos.at(mapping.renderTargetIndex);
            auto& renderTarget = *m_params.renderTargets.at(mapping.renderTargetIndex);
            if (mapping.bufferOverDepthSlices) {
                // TODO(nemanjab): fix this to have similar interface to layer-based buffering.
                framebufferLayerCount = renderTargetInfo.buffered
                                            ? renderTargetInfo.depthSlices / RendererConfig::VirtualFrameCount
                                            : renderTargetInfo.depthSlices;
                const uint32_t frameDepthOffset =
                    renderTargetInfo.buffered ? static_cast<uint32_t>(frameIdx) * framebufferLayerCount : 0;
                const VkImageViewType type =
                    framebufferLayerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                frameAttachmentViews[attachmentIdx] =
                    createView(renderTarget, type, frameDepthOffset, framebufferLayerCount);
            } else {
                // This handles the case where the render target isn't buffered. In that case, we create another
                // image view that essentially points to the same region as the virtual frame 0.
                const uint32_t frameLayerOffset =
                    renderTargetInfo.buffered ? static_cast<uint32_t>(frameIdx) * renderTargetInfo.layerCount : 0;
                const uint32_t firstLayer = frameLayerOffset + mapping.subresource.baseArrayLayer;
                const uint32_t layerCount = mapping.subresource.layerCount;
                const VkImageViewType type = layerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                frameAttachmentViews[attachmentIdx] = createView(
                    renderTarget,
                    type,
                    firstLayer,
                    layerCount,
                    mapping.subresource.baseMipLevel,
                    mapping.subresource.levelCount);
            }

            attachmentViewHandles[attachmentIdx] = frameAttachmentViews[attachmentIdx]->getHandle();
        }

        m_framebuffers[frameIdx] = std::make_unique<VulkanFramebuffer>(
            device, m_handle, m_params.renderArea, attachmentViewHandles, framebufferLayerCount);
    }
}

void VulkanRenderPass::createResources(const VulkanDevice& device) {
    m_framebuffers.resize(RendererConfig::VirtualFrameCount);
    if (m_params.allocateAttachmentViews) {
        createRenderTargetViewsAndFramebuffers(device);
    }
}

void VulkanRenderPass::freeResources() {
    m_attachmentViews.clear();
    m_framebuffers.clear();
}

} // namespace crisp
