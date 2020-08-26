#pragma once

#include "vulkan/vulkan.h"
#include <memory>

#include "vulkan/VulkanCommandPool.hpp"
#include "vulkan/VulkanCommandBuffer.hpp"
#include "vulkan/VulkanQueue.hpp"

namespace crisp
{
    struct VirtualFrame
    {
        std::unique_ptr<VulkanCommandPool>   cmdPool;
        std::unique_ptr<VulkanCommandBuffer> cmdBuffer;

        VkFence     bufferFinishedFence;
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;

        inline void waitCommandBuffer(VkDevice device)
        {
            vkWaitForFences(device, 1, &bufferFinishedFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
            vkResetFences(device,   1, &bufferFinishedFence);
        }

        inline void submit(const VulkanQueue* queue)
        {
            queue->submit(imageAvailableSemaphore, renderFinishedSemaphore, cmdBuffer->getHandle(), bufferFinishedFence);
        }
    };
}