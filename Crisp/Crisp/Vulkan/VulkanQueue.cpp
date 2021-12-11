#include "VulkanQueue.hpp"

#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanContext.hpp>

namespace crisp
{
    VulkanQueue::VulkanQueue(VulkanDevice* device, uint32_t familyIndex, uint32_t queueIndex)
        : m_device(device)
        , m_familyIndex(familyIndex)
        , m_index(queueIndex)
    {
        VkDeviceQueueInfo2 info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
        info.queueFamilyIndex = m_familyIndex;
        info.queueIndex = m_index;
        vkGetDeviceQueue2(m_device->getHandle(), &info, &m_handle);
    }

    VulkanQueue::VulkanQueue(VulkanDevice* device, QueueIdentifier queueId)
        : VulkanQueue(device, queueId.familyIndex, queueId.index)
    {
    }

    VkResult VulkanQueue::submit(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkCommandBuffer commandBuffer, VkFence fence) const
    {
        const VkPipelineStageFlags waitStage[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &waitSemaphore;
        submitInfo.pWaitDstStageMask    = waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &signalSemaphore;
        return vkQueueSubmit(m_handle, 1, &submitInfo, fence);
    }

    VkResult VulkanQueue::submit(VkCommandBuffer cmdBuffer, VkFence fence) const
    {
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &cmdBuffer;
        return vkQueueSubmit(m_handle, 1, &submitInfo, fence);
    }

    VkResult VulkanQueue::present(VkSemaphore waitSemaphore, VkSwapchainKHR VulkanSwapChain, uint32_t imageIndex) const
    {
        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &waitSemaphore;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &VulkanSwapChain;
        presentInfo.pImageIndices      = &imageIndex;
        presentInfo.pResults           = nullptr;
        return vkQueuePresentKHR(m_handle, &presentInfo);
    }

    void VulkanQueue::wait(VkFence fence) const
    {
        vkWaitForFences(m_device->getHandle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    void VulkanQueue::waitIdle() const
    {
        vkQueueWaitIdle(m_handle);
    }

    VkCommandPool VulkanQueue::createCommandPool(VkCommandPoolCreateFlags flags) const
    {
        VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.queueFamilyIndex = m_familyIndex;
        poolInfo.flags            = flags;

        VkCommandPool pool;
        vkCreateCommandPool(m_device->getHandle(), &poolInfo, nullptr, &pool);
        return pool;
    }

    uint32_t VulkanQueue::getFamilyIndex() const
    {
        return m_familyIndex;
    }

    bool VulkanQueue::supportsOperations(VkQueueFlags queueFlags) const
    {
        const auto properties = m_device->getPhysicalDevice().queryQueueFamilyProperties();
        return properties.at(m_familyIndex).queueFlags & queueFlags;
    }
}