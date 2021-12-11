#include <Crisp/Vulkan/VulkanCommandPool.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp
{
    VulkanCommandPool::VulkanCommandPool(VkCommandPool handle, VulkanResourceDeallocator& deallocator)
        : VulkanResource(handle, deallocator)
    {
    }

    VkCommandBuffer VulkanCommandPool::allocateCommandBuffer(const VulkanDevice* device, const VkCommandBufferLevel cmdBufferLevel) const
    {
        VkCommandBufferAllocateInfo cmdBufferAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cmdBufferAllocInfo.commandPool = m_handle;
        cmdBufferAllocInfo.level = cmdBufferLevel;
        cmdBufferAllocInfo.commandBufferCount = 1;

        VkCommandBuffer bufferHandle;
        vkAllocateCommandBuffers(device->getHandle(), &cmdBufferAllocInfo, &bufferHandle);
        return bufferHandle;
    }
}
