#include "VulkanQueue.hpp"

#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>

namespace crisp
{
VulkanQueue::VulkanQueue(
    const VkDevice deviceHandle, const VulkanPhysicalDevice& physicalDevice, QueueIdentifier queueId)
    : m_deviceHandle(deviceHandle)
    , m_familyIndex(queueId.familyIndex)
    , m_index(queueId.index)
    , m_familyProperties(physicalDevice.queryQueueFamilyProperties().at(m_familyIndex))
{
    VkDeviceQueueInfo2 info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2};
    info.queueFamilyIndex = m_familyIndex;
    info.queueIndex = m_index;
    vkGetDeviceQueue2(m_deviceHandle, &info, &m_handle);
}

VkResult VulkanQueue::submit(
    VkSemaphore waitSemaphore,
    VkSemaphore signalSemaphore,
    VkCommandBuffer commandBuffer,
    VkFence fence,
    VkPipelineStageFlags waitPipelineStage) const
{
    const VkPipelineStageFlags waitStage[] = {waitPipelineStage};

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    if (waitSemaphore != VK_NULL_HANDLE)
    {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.pWaitDstStageMask = waitStage;
    }
    if (signalSemaphore != VK_NULL_HANDLE)
    {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;
    }

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    return vkQueueSubmit(m_handle, 1, &submitInfo, fence);
}

VkResult VulkanQueue::submit(VkCommandBuffer cmdBuffer, VkFence fence) const
{
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    return vkQueueSubmit(m_handle, 1, &submitInfo, fence);
}

VkResult VulkanQueue::present(VkSemaphore waitSemaphore, VkSwapchainKHR VulkanSwapChain, uint32_t imageIndex) const
{
    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &VulkanSwapChain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    return vkQueuePresentKHR(m_handle, &presentInfo);
}

void VulkanQueue::waitIdle() const
{
    vkQueueWaitIdle(m_handle);
}

VkCommandPool VulkanQueue::createCommandPool(VkCommandPoolCreateFlags flags) const
{
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = m_familyIndex;
    poolInfo.flags = flags;

    VkCommandPool pool;
    vkCreateCommandPool(m_deviceHandle, &poolInfo, nullptr, &pool);
    return pool;
}

uint32_t VulkanQueue::getFamilyIndex() const
{
    return m_familyIndex;
}

bool VulkanQueue::supportsOperations(VkQueueFlags queueFlags) const
{
    return m_familyProperties.queueFlags & queueFlags;
}
} // namespace crisp