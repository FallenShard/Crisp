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

    void VulkanCommandBuffer::begin(VkCommandBufferUsageFlags commandBufferUsage)
    {
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = commandBufferUsage;
        beginInfo.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(m_handle, &beginInfo);
    }

    void VulkanCommandBuffer::begin(VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance)
    {
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = commandBufferUsage;
        beginInfo.pInheritanceInfo = inheritance;

        vkBeginCommandBuffer(m_handle, &beginInfo);
    }

    void VulkanCommandBuffer::end()
    {
        vkEndCommandBuffer(m_handle);
    }
}
