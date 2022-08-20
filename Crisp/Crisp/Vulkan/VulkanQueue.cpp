#include "VulkanQueue.hpp"

#include <Crisp/vulkan/VulkanContext.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>

namespace crisp
{
VulkanQueue::VulkanQueue(const VulkanDevice& device, uint32_t familyIndex, uint32_t queueIndex)
    : m_deviceHandle(device.getHandle())
    , m_familyIndex(familyIndex)
    , m_index(queueIndex)
    , m_familyProperties(device.getPhysicalDevice().queryQueueFamilyProperties().at(familyIndex))
{
    VkDeviceQueueInfo2 info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
    info.queueFamilyIndex = m_familyIndex;
    info.queueIndex = m_index;
    vkGetDeviceQueue2(m_deviceHandle, &info, &m_handle);
}

VulkanQueue::VulkanQueue(const VulkanDevice& device, QueueIdentifier queueId)
    : VulkanQueue(device, queueId.familyIndex, queueId.index)
{
}

VkResult VulkanQueue::submit(VkSemaphore waitSemaphore,
    VkSemaphore signalSemaphore,
    VkCommandBuffer commandBuffer,
    VkFence fence,
    VkPipelineStageFlags waitPipelineStage) const
{
    const VkPipelineStageFlags waitStage[] = { waitPipelineStage };

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
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
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    return vkQueueSubmit(m_handle, 1, &submitInfo, fence);
}

VkResult VulkanQueue::present(VkSemaphore waitSemaphore, VkSwapchainKHR VulkanSwapChain, uint32_t imageIndex) const
{
    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
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
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
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