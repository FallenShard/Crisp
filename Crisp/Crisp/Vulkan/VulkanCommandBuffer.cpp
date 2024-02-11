#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>

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

void VulkanCommandBuffer::transferOwnership(
    VkBuffer buffer, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const {
    VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    vkCmdPipelineBarrier(
        m_handle,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0,
        nullptr,
        1,
        &barrier,
        0,
        nullptr);
}

void VulkanCommandBuffer::insertMemoryBarrier(
    VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) const {
    VkMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.memoryBarrierCount = 1;
    info.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_handle, &info);
}

void VulkanCommandBuffer::insertBufferMemoryBarrier(
    const VkDescriptorBufferInfo& bufferInfo,
    VkPipelineStageFlags srcStage,
    VkAccessFlags srcAccess,
    VkPipelineStageFlags dstStage,
    VkAccessFlags dstAccess) const {
    VkBufferMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    barrier.buffer = bufferInfo.buffer;
    barrier.offset = bufferInfo.offset;
    barrier.size = bufferInfo.range;
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.bufferMemoryBarrierCount = 1;
    info.pBufferMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_handle, &info);
}

void VulkanCommandBuffer::insertBufferMemoryBarrier(
    const VkBufferMemoryBarrier& barrier, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const {
    vkCmdPipelineBarrier(m_handle, srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanCommandBuffer::insertBufferMemoryBarriers(
    std::span<VkBufferMemoryBarrier> barriers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const {
    vkCmdPipelineBarrier(
        m_handle, srcStage, dstStage, 0, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data(), 0, nullptr);
}

void VulkanCommandBuffer::insertImageMemoryBarrier(
    const VkImageMemoryBarrier& barrier, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const {
    vkCmdPipelineBarrier(m_handle, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// void VulkanCommandBuffer::insertImageMemoryBarrier(const VulkanBufferView& bufferView, VkPipelineStageFlags srcStage,
//     VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) const
//{
//     VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
//     barrier.srcAccessMask = srcAccess;
//     barrier.dstAccessMask = dstAccess;
//
//     vkCmdPipelineBarrier(m_handle, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
// }
//
// void VulkanCommandBuffer::insertMemoryBarrier(VkPipelineStageFlags srcStage, VkAccessFlags srcAccess,
//     VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) const
//{
//     vkCmdPipelineBarrier(m_handle, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 0, nullptr);
// }

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

void VulkanCommandBuffer::dispatchCompute(const glm::ivec3& workGroupCount) const {
    vkCmdDispatch(m_handle, workGroupCount.x, workGroupCount.y, workGroupCount.z);
}
} // namespace crisp
