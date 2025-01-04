#pragma once

#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueueConfiguration.hpp>

namespace crisp {
class VulkanQueue {
public:
    VulkanQueue(VkDevice deviceHandle, const VulkanPhysicalDevice& physicalDevice, QueueIdentifier queueId);

    void submit(
        VkSemaphore waitSemaphore,
        VkSemaphore signalSemaphore,
        VkCommandBuffer commandBuffer,
        VkFence fence,
        VkPipelineStageFlags waitPipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const;
    void submit(VkCommandBuffer cmdBuffer, VkFence fence = VK_NULL_HANDLE) const;
    VkResult present(VkSemaphore waitSemaphore, VkSwapchainKHR VulkanSwapChain, uint32_t imageIndex) const;

    void waitIdle() const;

    VkCommandPool createCommandPool(VkCommandPoolCreateFlags flags = 0) const;
    uint32_t getFamilyIndex() const;

    bool supportsOperations(VkQueueFlags queueFlags) const;

    VkQueue getHandle() const {
        return m_handle;
    }

    template <typename Func>
    void submitAndWait(const Func& func) const {
        const auto executionInfo = createExecutionInfo();
        func(executionInfo.commandBuffer);
        submitAndWait(executionInfo);
    }

private:
    struct ExecutionInfo {
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
    };

    ExecutionInfo createExecutionInfo() const;
    void submitAndWait(const ExecutionInfo& executionInfo) const;

    VkQueue m_handle; // Deallocated automatically once the parent device is destroyed.
    VkDevice m_deviceHandle;
    uint32_t m_familyIndex;
    uint32_t m_index;
    VkQueueFamilyProperties m_familyProperties;
};
} // namespace crisp