#pragma once

#include "VulkanResource.hpp"
#include "vulkan/VulkanQueueConfiguration.hpp"

namespace crisp
{
    class VulkanDevice;

    class VulkanQueue : public VulkanResource<VkQueue>
    {
    public:
        VulkanQueue(VulkanDevice* device, uint32_t familyIndex, uint32_t queueIndex);
        VulkanQueue(VulkanDevice* device, QueueIdentifier queueId);
        ~VulkanQueue();

        VkResult submit(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkCommandBuffer commandBuffer, VkFence fence) const;
        VkResult submit(VkCommandBuffer cmdBuffer) const;
        VkResult present(VkSemaphore waitSemaphore, VkSwapchainKHR VulkanSwapChain, uint32_t imageIndex) const;

        void waitIdle() const;

        VkCommandPool createCommandPool(VkCommandPoolCreateFlags flags) const;
        uint32_t getFamilyIndex() const;

    private:
        uint32_t      m_familyIndex;
        uint32_t      m_index;
    };
}