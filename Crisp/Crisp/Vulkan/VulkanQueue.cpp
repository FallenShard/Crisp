#include <Crisp/Vulkan/VulkanQueue.hpp>

#include <Crisp/Vulkan/VulkanChecks.hpp>

namespace crisp {
VulkanQueue::VulkanQueue(
    const VkDevice deviceHandle, const VulkanPhysicalDevice& physicalDevice, const QueueIdentifier queueId)
    : m_handle{VK_NULL_HANDLE}
    , m_deviceHandle(deviceHandle)
    , m_familyIndex(queueId.familyIndex)
    , m_index(queueId.index)
    , m_familyProperties(physicalDevice.queryQueueFamilyProperties().at(m_familyIndex)) {
    VkDeviceQueueInfo2 info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2};
    info.queueFamilyIndex = m_familyIndex;
    info.queueIndex = m_index;
    vkGetDeviceQueue2(m_deviceHandle, &info, &m_handle);
}

void VulkanQueue::submit(
    const VkSemaphore waitSemaphore,
    const VkSemaphore signalSemaphore,
    const VkCommandBuffer commandBuffer,
    const VkFence fence,
    const VkPipelineStageFlags waitPipelineStage) const {
    const std::array<VkPipelineStageFlags, 1> waitStages{waitPipelineStage};

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    if (waitSemaphore != VK_NULL_HANDLE) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.pWaitDstStageMask = waitStages.data();
    }
    if (signalSemaphore != VK_NULL_HANDLE) {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;
    }

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(m_handle, 1, &submitInfo, fence));
}

void VulkanQueue::submit(const VkCommandBuffer cmdBuffer, const VkFence fence) const {
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    VK_CHECK(vkQueueSubmit(m_handle, 1, &submitInfo, fence));
}

VkResult VulkanQueue::present(
    const VkSemaphore waitSemaphore, const VkSwapchainKHR swapChain, const uint32_t imageIndex) const {
    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    return vkQueuePresentKHR(m_handle, &presentInfo);
}

void VulkanQueue::waitIdle() const {
    VK_CHECK(vkQueueWaitIdle(m_handle));
}

VkCommandPool VulkanQueue::createCommandPool(const VkCommandPoolCreateFlags flags) const {
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = m_familyIndex;
    poolInfo.flags = flags;

    VkCommandPool pool{VK_NULL_HANDLE};
    VK_CHECK(vkCreateCommandPool(m_deviceHandle, &poolInfo, nullptr, &pool));
    return pool;
}

uint32_t VulkanQueue::getFamilyIndex() const {
    return m_familyIndex;
}

bool VulkanQueue::supportsOperations(const VkQueueFlags queueFlags) const {
    return m_familyProperties.queueFlags & queueFlags;
}
} // namespace crisp