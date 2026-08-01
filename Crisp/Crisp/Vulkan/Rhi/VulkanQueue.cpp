#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

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
    const VkPipelineStageFlags2 waitPipelineStage,
    const VkPipelineStageFlags2 signalPipelineStage) const {
    const VkSemaphoreSubmitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = waitSemaphore,
        .stageMask = waitPipelineStage,
    };
    const VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = signalSemaphore,
        .stageMask = signalPipelineStage,
    };
    const VkCommandBufferSubmitInfo cmdBufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer,
    };

    VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    if (waitSemaphore != VK_NULL_HANDLE) {
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitInfo;
    }
    if (signalSemaphore != VK_NULL_HANDLE) {
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalInfo;
    }
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdBufferInfo;

    VK_FATAL(vkQueueSubmit2(m_handle, 1, &submitInfo, fence));
}

void VulkanQueue::submit(const VkCommandBuffer cmdBuffer, const VkFence fence) const {
    const VkCommandBufferSubmitInfo cmdBufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdBuffer,
    };

    VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdBufferInfo;
    VK_FATAL(vkQueueSubmit2(m_handle, 1, &submitInfo, fence));
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

uint32_t VulkanQueue::getTimestampValidBits() const {
    return m_familyProperties.timestampValidBits;
}

bool VulkanQueue::supportsOperations(const VkQueueFlags queueFlags) const {
    return m_familyProperties.queueFlags & queueFlags;
}

VulkanQueue::ExecutionInfo VulkanQueue::createExecutionInfo() const {
    VkCommandPool cmdPool = createCommandPool(0);

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdBufferAllocInfo.commandPool = cmdPool;
    cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer{VK_NULL_HANDLE};
    VK_FATAL(vkAllocateCommandBuffers(m_deviceHandle, &cmdBufferAllocInfo, &cmdBuffer));

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_FATAL(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
    return {
        .commandPool = cmdPool,
        .commandBuffer = cmdBuffer,
    };
}

void VulkanQueue::submitAndWait(const ExecutionInfo& executionInfo) const {
    VK_FATAL(vkEndCommandBuffer(executionInfo.commandBuffer));
    submit(executionInfo.commandBuffer);
    VK_FATAL(vkQueueWaitIdle(m_handle));

    vkFreeCommandBuffers(m_deviceHandle, executionInfo.commandPool, 1, &executionInfo.commandBuffer);
    vkResetCommandPool(m_deviceHandle, executionInfo.commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    vkDestroyCommandPool(m_deviceHandle, executionInfo.commandPool, nullptr);
}

} // namespace crisp
