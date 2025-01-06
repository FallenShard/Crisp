#include <Crisp/Vulkan/Rhi/VulkanCommandPool.hpp>

namespace crisp {
VulkanCommandPool::VulkanCommandPool(VkCommandPool handle, VulkanResourceDeallocator& deallocator)
    : VulkanResource(handle, deallocator) {}

VkCommandBuffer VulkanCommandPool::allocateCommandBuffer(
    const VulkanDevice& device, const VkCommandBufferLevel cmdBufferLevel) const {
    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdBufferAllocInfo.commandPool = m_handle;
    cmdBufferAllocInfo.level = cmdBufferLevel;
    cmdBufferAllocInfo.commandBufferCount = 1;

    VkCommandBuffer bufferHandle{VK_NULL_HANDLE};
    vkAllocateCommandBuffers(device.getHandle(), &cmdBufferAllocInfo, &bufferHandle);
    return bufferHandle;
}

void VulkanCommandPool::reset(const VulkanDevice& device) {
    vkResetCommandPool(device.getHandle(), m_handle, 0);
}
} // namespace crisp
