#include "VulkanCommandBuffer.hpp"

#include "VulkanCommandPool.hpp"
#include "VulkanDevice.hpp"

namespace crisp
{
    VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer commandBuffer)
        : m_handle(commandBuffer)
    {
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
    }

    void VulkanCommandBuffer::begin(VkCommandBufferUsageFlags commandBufferUsage) const
    {
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = commandBufferUsage;
        beginInfo.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(m_handle, &beginInfo);
    }

    void VulkanCommandBuffer::begin(VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance) const
    {
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = commandBufferUsage;
        beginInfo.pInheritanceInfo = inheritance;

        vkBeginCommandBuffer(m_handle, &beginInfo);
    }

    void VulkanCommandBuffer::end() const
    {
        vkEndCommandBuffer(m_handle);
    }

    void VulkanCommandBuffer::transferOwnership(VkBuffer buffer, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const
    {
        VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.buffer = buffer;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
        barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
        vkCmdPipelineBarrier(m_handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    void VulkanCommandBuffer::insertPipelineBarrier(const VulkanBufferView& bufferView, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) const
    {
        VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.buffer = bufferView.handle;
        barrier.offset = bufferView.offset;
        barrier.size = bufferView.size;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(m_handle, srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    void VulkanCommandBuffer::copyBuffer(const VulkanBufferView& srcBufferView, VkBuffer dstBuffer) const
    {
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = srcBufferView.offset;
        copyRegion.dstOffset = 0;
        copyRegion.size = srcBufferView.size;
        vkCmdCopyBuffer(m_handle, srcBufferView.handle, dstBuffer, 1, &copyRegion);
    }
}
