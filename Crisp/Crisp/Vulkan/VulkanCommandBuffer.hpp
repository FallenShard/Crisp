#pragma once

#include <vulkan/vulkan.h>

#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
    class VulkanCommandPool;

    class VulkanCommandBuffer
    {
    public:
        VulkanCommandBuffer(const VulkanCommandPool* commandPool, VkCommandBufferLevel level);
        VulkanCommandBuffer(VkCommandBuffer commandBuffer);
        ~VulkanCommandBuffer();

        void begin(VkCommandBufferUsageFlags commandBufferUsage);
        void begin(VkCommandBufferUsageFlags commandBufferUsage, const VkCommandBufferInheritanceInfo* inheritance);

        void end();

        inline VkCommandBuffer getHandle() const { return m_handle; }

    private:
        VkCommandBuffer m_handle;
    };
}