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

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);

    image.setImageLayout(newLayout, range);
}

} // namespace crisp