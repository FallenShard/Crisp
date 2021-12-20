#pragma once

#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

namespace crisp
{
class VulkanDevice;

class VulkanQueue
{
public:
    VulkanQueue(const VulkanDevice& device, uint32_t familyIndex, uint32_t queueIndex);
    VulkanQueue(const VulkanDevice& device, QueueIdentifier queueId);

    VkResult submit(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkCommandBuffer commandBuffer,
        VkFence fence, VkPipelineStageFlags waitPipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const;
    VkResult submit(VkCommandBuffer cmdBuffer, VkFence fence = VK_NULL_HANDLE) const;
    VkResult present(VkSemaphore waitSemaphore, VkSwapchainKHR VulkanSwapChain, uint32_t imageIndex) const;

    void waitIdle() const;

    VkCommandPool createCommandPool(VkCommandPoolCreateFlags flags) const;
    uint32_t getFamilyIndex() const;

    bool supportsOperations(VkQueueFlags queueFlags) const;

    inline VkQueue getHandle() const
    {
        return m_handle;
    }

private:
    VkQueue m_handle;
    VkDevice m_deviceHandle;
    uint32_t m_familyIndex;
    uint32_t m_index;
    VkQueueFamilyProperties m_familyProperties;
};
} // namespace crisp