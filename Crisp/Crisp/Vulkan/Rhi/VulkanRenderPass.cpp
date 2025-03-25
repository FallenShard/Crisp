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
}

void VulkanRenderPass::setRenderArea(const VkExtent2D& extent) {
    m_params.renderArea = extent;
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

void VulkanRenderPass::begin(
    const VkCommandBuffer cmdBuffer, const VulkanFramebuffer& framebuffer, const VkSubpassContents content) const {
    VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = framebuffer.getHandle();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_params.renderArea;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(m_params.clearValues.size());
    renderPassInfo.pClearValues = m_params.clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, content);
}

void VulkanRenderPass::end(const VkCommandBuffer cmdBuffer) {
    vkCmdEndRenderPass(cmdBuffer);

    for (const auto& [i, attachmentView] : std::views::enumerate(m_attachmentViews)) {
        // We unconditionally set the layout here because the render pass did an automatic layout transition.
        attachmentView->getImage().setImageLayout(
            m_params.attachmentDescriptions.at(i).finalLayout, attachmentView->getSubresourceRange());
    }
}

void VulkanRenderPass::nextSubpass(const VkCommandBuffer cmdBuffer, const VkSubpassContents contents) const { // NOLINT
    vkCmdNextSubpass(cmdBuffer, contents);
}

VulkanImage& VulkanRenderPass::getRenderTarget(const uint32_t attachmentIndex) const {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachmentViews.size());
    return m_attachmentViews[attachmentIndex]->getImage();
}

const VulkanImageView& VulkanRenderPass::getAttachmentView(const uint32_t attachmentIndex) const {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachmentViews.size());
    return *m_attachmentViews[attachmentIndex];
}

} // namespace crisp
