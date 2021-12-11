#pragma once

#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

namespace crisp
{
    class VulkanDevice;

    class VulkanQueue
    {
    public:
        VulkanQueue(VulkanDevice* device, uint32_t familyIndex, uint32_t queueIndex);
        VulkanQueue(VulkanDevice* device, QueueIdentifier queueId);

        VkResult submit(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkCommandBuffer commandBuffer, VkFence fence) const;
        VkResult submit(VkCommandBuffer cmdBuffer, VkFence fence = VK_NULL_HANDLE) const;
        VkResult present(VkSemaphore waitSemaphore, VkSwapchainKHR VulkanSwapChain, uint32_t imageIndex) const;

        void wait(VkFence fence) const;
        void waitIdle() const;

        VkCommandPool createCommandPool(VkCommandPoolCreateFlags flags) const;
        uint32_t getFamilyIndex() const;
        
        bool supportsOperations(VkQueueFlags queueFlags) const;

        inline VkQueue getHandle() const { return m_handle; }

    private:
        VkQueue m_handle;
        VulkanDevice* m_device;
        uint32_t      m_familyIndex;
        uint32_t      m_index;
    };
}