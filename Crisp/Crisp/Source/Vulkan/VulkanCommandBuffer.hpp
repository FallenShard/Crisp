#pragma once

#include <vulkan/vulkan.h>

#include "vulkan/VulkanResource.hpp"

namespace crisp
{
    class VulkanCommandPool;

    class VulkanCommandBuffer
    {
    public:
        VulkanCommandBuffer(const VulkanCommandPool* commandPool, VkCommandBufferLevel level);
        VulkanCommandBuffer(VkCommandBuffer commandBuffer);
        ~VulkanCommandBuffer();

        inline VkCommandBuffer getHandle() const { return m_handle; }

    private:
        VkCommandBuffer m_handle;
    };
}