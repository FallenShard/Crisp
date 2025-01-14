#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

#include <ranges>

#include <Crisp/Core/Checks.hpp>

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
        1.0f,
    };
}

VkRect2D VulkanRenderPass::createScissor() const {
    static constexpr VkOffset2D kZero{0, 0};
    return {kZero, m_params.renderArea};
}

void VulkanRenderPass::begin(const VkCommandBuffer cmdBuffer, const VkSubpassContents content) const {
    VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = m_framebuffer->getHandle();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_params.renderArea;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(m_attachmentClearValues.size());
    renderPassInfo.pClearValues = m_attachmentClearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, content);
}

void VulkanRenderPass::begin(
    const VkCommandBuffer cmdBuffer, const VulkanFramebuffer& framebuffer, const VkSubpassContents content) const {
    VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = framebuffer.getHandle();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_params.renderArea;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(m_attachmentClearValues.size());
    renderPassInfo.pClearValues = m_attachmentClearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, content);
}

void VulkanRenderPass::end(const VkCommandBuffer cmdBuffer) {
    vkCmdEndRenderPass(cmdBuffer);

    if (m_attachmentViews.empty()) {
        return;
    }

    for (const auto& [i, attachmentView] : std::views::enumerate(m_attachmentViews)) {
        const uint32_t renderTargetIndex{m_params.attachmentMappings.at(i).renderTargetIndex};
        auto& image = *m_params.renderTargets.at(renderTargetIndex);

        // We unconditionally set the layout here because the render pass did an automatic layout transition.
        image.setImageLayout(m_params.attachmentDescriptions.at(i).finalLayout, attachmentView->getSubresourceRange());
    }
}

void VulkanRenderPass::nextSubpass(const VkCommandBuffer cmdBuffer, const VkSubpassContents contents) const { // NOLINT
    vkCmdNextSubpass(cmdBuffer, contents);
}

VulkanImage& VulkanRenderPass::getRenderTarget(const uint32_t index) const {
    return *m_params.renderTargets.at(index);
}

VulkanImage& VulkanRenderPass::getRenderTargetFromAttachmentIndex(const uint32_t attachmentIndex) const {
    const uint32_t renderTargetIndex{m_params.attachmentMappings.at(attachmentIndex).renderTargetIndex};
    return *m_params.renderTargets.at(renderTargetIndex);
}

const VulkanImageView& VulkanRenderPass::getAttachmentView(const uint32_t attachmentIndex) const {
    return *m_attachmentViews.at(attachmentIndex);
}

void VulkanRenderPass::updateInitialLayouts(VkCommandBuffer cmdBuffer) {
    for (const auto& [i, attachmentView] : std::views::enumerate(m_attachmentViews)) {
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

void VulkanRenderPass::createAttachmentViews(const VulkanDevice& device) {
    m_attachmentViews.resize(m_params.attachmentDescriptions.size());

    for (const auto& [attachmentIdx, mapping] : std::views::enumerate(m_params.attachmentMappings)) {
        // const auto& renderTargetInfo = m_params.renderTargetInfos.at(mapping.renderTargetIndex);
        auto& renderTarget = *m_params.renderTargets.at(mapping.renderTargetIndex);
        if (mapping.bufferOverDepthSlices) {
            // TODO(nemanjab): fix this to have similar interface to layer-based buffering.
            CRISP_FATAL("Whooops");
            // framebufferLayerCount =
            //     renderTargetInfo.buffered
            //         ? renderTargetInfo.depthSlices / kRendererVirtualFrameCount
            //         : renderTargetInfo.depthSlices;
            // const uint32_t frameDepthOffset =
            //     renderTargetInfo.buffered ? static_cast<uint32_t>(frameIdx) * framebufferLayerCount : 0;
            // const VkImageViewType type =
            //     framebufferLayerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            // frameAttachmentViews[attachmentIdx] =
            //     createView(device, renderTarget, type, frameDepthOffset, framebufferLayerCount);
        } else {
            // This handles the case where the render target isn't buffered. In that case, we create another
            // image view that essentially points to the same region as the virtual frame 0.
            m_attachmentViews[attachmentIdx] = createView(
                device,
                renderTarget,
                mapping.subresource.layerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                mapping.subresource.baseArrayLayer,
                mapping.subresource.layerCount,
                mapping.subresource.baseMipLevel,
                mapping.subresource.levelCount);
        }
    }
}

void VulkanRenderPass::createResources(const VulkanDevice& device) {
    if (m_params.allocateAttachmentViews) {
        createAttachmentViews(device);

        std::vector<VkImageView> attachmentViewHandles{};
        attachmentViewHandles.reserve(m_attachmentViews.size());
        for (const auto& view : m_attachmentViews) {
            attachmentViewHandles.push_back(view->getHandle());
        }

        constexpr uint32_t kFramebufferLayerCount{1};
        m_framebuffer = std::make_unique<VulkanFramebuffer>(
            device, m_handle, m_params.renderArea, attachmentViewHandles, kFramebufferLayerCount);
    }
}

void VulkanRenderPass::freeResources() {
    m_attachmentViews.clear();
    m_framebuffer.reset();
}

} // namespace crisp
