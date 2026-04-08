#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>

namespace crisp {
VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer commandBuffer)
    : m_handle(commandBuffer)
    , m_state(State::Idle) {}

void VulkanCommandBuffer::setIdleState() {
    CRISP_CHECK(m_state == State::Executing || m_state == State::Idle);
    m_state = State::Idle;
}

void VulkanCommandBuffer::begin(const VkCommandBufferUsageFlags commandBufferUsage) {
    CRISP_CHECK_EQ(m_state, State::Idle);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = commandBufferUsage;
    beginInfo.pInheritanceInfo = nullptr;

    vkBeginCommandBuffer(m_handle, &beginInfo);
    m_state = State::Recording;
}

void VulkanCommandBuffer::begin(
    const VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance) {
    CRISP_CHECK_EQ(m_state, State::Idle);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = commandBufferUsage;
    beginInfo.pInheritanceInfo = inheritance;

    vkBeginCommandBuffer(m_handle, &beginInfo);
    m_state = State::Recording;
}

void VulkanCommandBuffer::end() {
    CRISP_CHECK_EQ(m_state, State::Recording);
    vkEndCommandBuffer(m_handle);
    m_state = State::Pending;
}

void VulkanCommandBuffer::setExecutionState() {
    CRISP_CHECK_EQ(m_state, State::Pending);
    m_state = State::Executing;
}

void VulkanCommandBuffer::insertBarrier(const VulkanSynchronizationScope& scope) const {
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.memoryBarrierCount = 1;
    info.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_handle, &info);
}

void VulkanCommandBuffer::insertBufferMemoryBarrier(
    const VkBuffer buffer,
    const VkDeviceSize offset,
    const VkDeviceSize size,
    const VulkanSynchronizationScope& scope) const {
    VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.bufferMemoryBarrierCount = 1;
    info.pBufferMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_handle, &info);
}

void VulkanCommandBuffer::insertBufferMemoryBarrier(
    const VkDescriptorBufferInfo& bufferInfo, const VulkanSynchronizationScope& scope) const {
    insertBufferMemoryBarrier(bufferInfo.buffer, bufferInfo.offset, bufferInfo.range, scope);
}

void VulkanCommandBuffer::insertBufferMemoryBarriers(
    const std::span<const VkBufferMemoryBarrier2> barriers) const {
    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    info.pBufferMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(m_handle, &info);
}

void VulkanCommandBuffer::insertImageMemoryBarrier(const VkImageMemoryBarrier2& barrier) const {
    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_handle, &info);
}

void VulkanCommandBuffer::transferOwnership(
    const VkBuffer buffer,
    const uint32_t srcQueueFamilyIndex,
    const uint32_t dstQueueFamilyIndex,
    const VulkanSynchronizationScope& scope) const {
    VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.bufferMemoryBarrierCount = 1;
    info.pBufferMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_handle, &info);
}

void VulkanCommandBuffer::executeSecondaryBuffers(const std::vector<VkCommandBuffer>& commandBuffers) const {
    vkCmdExecuteCommands(m_handle, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
}

void VulkanCommandBuffer::updateBuffer(
    const VkDescriptorBufferInfo& bufferInfo, const MemoryRegion& memoryRegion) const {
    vkCmdUpdateBuffer(m_handle, bufferInfo.buffer, bufferInfo.offset, bufferInfo.range, memoryRegion.ptr);
}

void VulkanCommandBuffer::copyBuffer(const VkDescriptorBufferInfo& srcBufferInfo, VkBuffer dstBuffer) const {
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = srcBufferInfo.offset;
    copyRegion.dstOffset = 0;
    copyRegion.size = srcBufferInfo.range;
    vkCmdCopyBuffer(m_handle, srcBufferInfo.buffer, dstBuffer, 1, &copyRegion);
}

void VulkanCommandBuffer::dispatchCompute(const VkExtent3D& workGroupCount) const {
    vkCmdDispatch(m_handle, workGroupCount.width, workGroupCount.height, workGroupCount.depth);
}
} // namespace crisp
