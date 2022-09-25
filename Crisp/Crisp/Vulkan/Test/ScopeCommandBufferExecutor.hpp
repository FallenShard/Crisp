#pragma once

#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

class ScopeCommandBufferExecutor
{
public:
    inline ScopeCommandBufferExecutor(const crisp::VulkanDevice& device)
        : device(device)
        , commandPool(device.getGeneralQueue().createCommandPool(0), device.getResourceDeallocator())
        , cmdBuffer(commandPool.allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
        , fence(device.createFence(0))
    {
        cmdBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    inline ~ScopeCommandBufferExecutor()
    {
        cmdBuffer.end();
        device.getGeneralQueue().submit(cmdBuffer.getHandle(), fence);
        device.wait(fence);
        vkDestroyFence(device.getHandle(), fence, nullptr);
    }

    ScopeCommandBufferExecutor(ScopeCommandBufferExecutor&&) = delete;
    ScopeCommandBufferExecutor(const ScopeCommandBufferExecutor&) = delete;

    const crisp::VulkanDevice& device;
    const crisp::VulkanCommandPool commandPool;
    crisp::VulkanCommandBuffer cmdBuffer;
    VkFence fence;
};