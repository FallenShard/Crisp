#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>

namespace crisp
{
VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer commandBuffer)
    : m_handle(commandBuffer)
{
}

VulkanCommandBuffer::~VulkanCommandBuffer() {}

void VulkanCommandBuffer::begin(VkCommandBufferUsageFlags commandBufferUsage) const
{
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = commandBufferUsage;
    beginInfo.pInheritanceInfo = nullptr;

    vkBeginCommandBuffer(m_handle, &beginInfo);
}

void VulkanCommandBuffer::begin(
    VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance) const
{
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = commandBufferUsage;
    beginInfo.pInheritanceInfo = inheritance;

    vkBeginCommandBuffer(m_handle, &beginInfo);
}

void VulkanCommandBuffer::end() const
{
    vkEndCommandBuffer(m_handle);
}

void VulkanCommandBuffer::transferOwnership(
    VkBuffer buffer, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const
{
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

void VulkanCommandBuffer::insertBufferMemoryBarrier(
    const VulkanBufferSpan& bufferSpan,
    VkPipelineStageFlags srcStage,
    VkAccessFlags srcAccess,
    VkPipelineStageFlags dstStage,
    VkAccessFlags dstAccess) const
{
    VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.buffer = bufferSpan.handle;
    barrier.offset = bufferSpan.offset;
    barrier.size = bufferSpan.size;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(m_handle, srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanCommandBuffer::insertBufferMemoryBarrier(
    const VkBufferMemoryBarrier& barrier, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const
{
    vkCmdPipelineBarrier(m_handle, srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanCommandBuffer::insertBufferMemoryBarriers(
    std::span<VkBufferMemoryBarrier> barriers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const
{
    vkCmdPipelineBarrier(
        m_handle,
        srcStage,
        dstStage,
        0,
        0,
        nullptr,
        static_cast<uint32_t>(barriers.size()),
        barriers.data(),
        0,
        nullptr);
}

void VulkanCommandBuffer::insertImageMemoryBarrier(
    const VkImageMemoryBarrier& barrier, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const
{
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

void VulkanCommandBuffer::executeSecondaryBuffers(const std::vector<VkCommandBuffer>& commandBuffers) const
{
    vkCmdExecuteCommands(m_handle, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
}

void VulkanCommandBuffer::updateBuffer(const VulkanBufferSpan& bufferSpan, const MemoryRegion& memoryRegion) const
{
    vkCmdUpdateBuffer(m_handle, bufferSpan.handle, bufferSpan.offset, bufferSpan.size, memoryRegion.ptr);
}

void VulkanCommandBuffer::copyBuffer(const VulkanBufferSpan& srcBufferSpan, VkBuffer dstBuffer) const
{
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = srcBufferSpan.offset;
    copyRegion.dstOffset = 0;
    copyRegion.size = srcBufferSpan.size;
    vkCmdCopyBuffer(m_handle, srcBufferSpan.handle, dstBuffer, 1, &copyRegion);
}

void VulkanCommandBuffer::dispatchCompute(const glm::ivec3& workGroupCount) const
{
    vkCmdDispatch(m_handle, workGroupCount.x, workGroupCount.y, workGroupCount.z);
}
} // namespace crisp
