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

} // namespace crisp