#include "VulkanQueue.hpp"

#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanContext.hpp"

namespace crisp
{
    VulkanQueue::VulkanQueue(VulkanDevice* device, uint32_t familyIndex, uint32_t queueIndex)
        : m_device(device)
        , m_queue(VK_NULL_HANDLE)
        , m_familyIndex(familyIndex)
        , m_index(queueIndex)
    {
        vkGetDeviceQueue(m_device->getHandle(), m_familyIndex, m_index, &m_queue);
    }

    VulkanQueue::VulkanQueue(VulkanDevice* device, QueueIdentifier queueId)
        : VulkanQueue(device, queueId.familyIndex, queueId.index)
    {
    }
    
    VulkanQueue::~VulkanQueue()
    {
    }

    VkResult VulkanQueue::submit(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkCommandBuffer commandBuffer, VkFence fence) const
    {
        VkPipelineStageFlags waitStage[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &waitSemaphore;
        submitInfo.pWaitDstStageMask    = waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &signalSemaphore;
        return vkQueueSubmit(m_queue, 1, &submitInfo, fence);
    }

    VkResult VulkanQueue::submit(VkCommandBuffer cmdBuffer) const
    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &cmdBuffer;
        return vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    }

    VkResult VulkanQueue::present(VkSemaphore waitSemaphore, VkSwapchainKHR swapChain, uint32_t imageIndex) const
    {
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &waitSemaphore;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &swapChain;
        presentInfo.pImageIndices      = &imageIndex;
        presentInfo.pResults           = nullptr;
        return vkQueuePresentKHR(m_queue, &presentInfo);
    }

    void VulkanQueue::waitIdle() const
    {
        vkQueueWaitIdle(m_queue);
    }

    VkCommandPool VulkanQueue::createCommandPoolFromFamily(VkCommandPoolCreateFlags flags) const
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
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
}