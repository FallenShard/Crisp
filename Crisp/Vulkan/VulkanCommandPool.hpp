#pragma once

#include <vulkan/vulkan.h>
#include "VulkanResource.hpp"

#include <memory>

namespace crisp
{
    class VulkanQueue;
    class VulkanCommandBuffer;

    class VulkanCommandPool : public VulkanResource<VkCommandPool>
    {
    public:
        VulkanCommandPool(const VulkanQueue& vulkanQueue, VkCommandPoolCreateFlags flags);
        ~VulkanCommandPool();

        std::unique_ptr<VulkanCommandBuffer> allocateCommandBuffer(VkCommandBufferLevel level) const;
    };
}