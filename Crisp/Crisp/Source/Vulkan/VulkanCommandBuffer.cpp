#include "VulkanCommandBuffer.hpp"

#include "VulkanCommandPool.hpp"
#include "VulkanDevice.hpp"

namespace crisp
{
    VulkanCommandBuffer::VulkanCommandBuffer(const VulkanCommandPool* commandPool, VkCommandBufferLevel level)
        : m_handle(VK_NULL_HANDLE)
    {
        VkCommandBufferAllocateInfo cmdBufferAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cmdBufferAllocInfo.commandPool                 = commandPool->getHandle();
        cmdBufferAllocInfo.level                       = level;
        cmdBufferAllocInfo.commandBufferCount          = 1;
        vkAllocateCommandBuffers(commandPool->getDevice()->getHandle(), &cmdBufferAllocInfo, &m_handle);
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer commandBuffer)
        : m_handle(commandBuffer)
    {
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
    }
}
