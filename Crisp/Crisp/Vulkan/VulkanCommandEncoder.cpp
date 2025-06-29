#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

namespace crisp {

VulkanCommandEncoder::VulkanCommandEncoder(const VkCommandBuffer cmdBuffer)
    : m_cmdBuffer(cmdBuffer) {}

void VulkanCommandEncoder::setViewport(const VkViewport& viewport) const {
    vkCmdSetViewport(m_cmdBuffer, 0, 1, &viewport);
}

void VulkanCommandEncoder::setScissor(const VkRect2D& scissorRect) const {
    vkCmdSetScissor(m_cmdBuffer, 0, 1, &scissorRect);
}

void VulkanCommandEncoder::bindPipeline(const VulkanPipeline& pipeline) const {
    vkCmdBindPipeline(m_cmdBuffer, pipeline.getBindPoint(), pipeline.getHandle());
}

void VulkanCommandEncoder::insertBarrier(const VulkanSynchronizationScope& scope) const {
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.memoryBarrierCount = 1;
    info.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
}

void VulkanCommandEncoder::insertBufferMemoryBarrier(
    const VulkanBuffer& buffer, const VulkanSynchronizationScope& scope) const {
    VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    barrier.buffer = buffer.getHandle();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
}

void VulkanCommandEncoder::transitionLayout(
    VulkanImage& image, const VkImageLayout newLayout, const VulkanSynchronizationScope& scope) const {
    const auto range = image.getFullRange();

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.oldLayout = image.getLayout();
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.getHandle();
    barrier.subresourceRange = range;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
    image.setImageLayout(newLayout, range);
}

void VulkanCommandEncoder::transitionLayout(
    VulkanImage& image,
    const VkImageLayout newLayout,
    const VulkanSynchronizationScope& scope,
    const VkImageSubresourceRange& range) const {
    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.oldLayout = image.getLayout(range.baseArrayLayer, range.baseMipLevel);
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.getHandle();
    barrier.subresourceRange = range;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    const VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
    image.setImageLayout(newLayout, range);
}

void VulkanCommandEncoder::beginRenderPass(
    const VulkanRenderPass& renderPass, const VulkanFramebuffer& framebuffer) const {
    const auto& clearValues = renderPass.getClearValues();

    VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = renderPass.getHandle();
    renderPassInfo.framebuffer = framebuffer.getHandle();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = renderPass.getRenderArea();
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandEncoder::endRenderPass(VulkanRenderPass& renderPass) const {
    renderPass.end(m_cmdBuffer);
}

void VulkanCommandEncoder::copyImageToBuffer(const VulkanImage& srcImage, const VulkanBuffer& dstBuffer) const {
    const VkDeviceSize size = srcImage.getWidth() * srcImage.getHeight() * 4 * sizeof(float);
    CRISP_CHECK_EQ(size, dstBuffer.getSize());
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {.width = srcImage.getWidth(), .height = srcImage.getHeight(), .depth = 1};
    vkCmdCopyImageToBuffer(
        m_cmdBuffer, srcImage.getHandle(), VK_IMAGE_LAYOUT_GENERAL, dstBuffer.getHandle(), 1, &region);
}

void VulkanCommandEncoder::traceRays(
    const std::span<const VkStridedDeviceAddressRegionKHR> bindingRegions, const VkExtent2D& gridSize) const {
    CRISP_CHECK_GE(bindingRegions.size(), 4);
    vkCmdTraceRaysKHR(
        m_cmdBuffer,
        &bindingRegions[0], // Raygen NOLINT
        &bindingRegions[1], // Miss
        &bindingRegions[2], // Hit
        &bindingRegions[3], // Callable
        gridSize.width,
        gridSize.height,
        1);
}

} // namespace crisp