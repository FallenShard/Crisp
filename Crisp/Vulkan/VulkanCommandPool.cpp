#include "VulkanCommandPool.hpp"

#include "VulkanQueue.hpp"
#include "VulkanDevice.hpp"
#include "VulkanCommandBuffer.hpp"

namespace crisp
{
    VulkanCommandPool::VulkanCommandPool(const VulkanQueue& vulkanQueue, VkCommandPoolCreateFlags flags)
        : VulkanResource(vulkanQueue.getDevice())
    {
        VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.queueFamilyIndex = vulkanQueue.getFamilyIndex();
        poolInfo.flags            = flags;

        vkCreateCommandPool(m_device->getHandle(), &poolInfo, nullptr, &m_handle);
    }

    VulkanCommandPool::~VulkanCommandPool()
    {
        m_device->deferDestruction(m_framesToLive, m_handle, [](void* handle, VkDevice device)
        {
            vkDestroyCommandPool(device, static_cast<VkCommandPool>(handle), nullptr);
        });
    }

    std::unique_ptr<VulkanCommandBuffer> VulkanCommandPool::allocateCommandBuffer(VkCommandBufferLevel level) const
    {
        return std::make_unique<VulkanCommandBuffer>(this, level);
    }
}
