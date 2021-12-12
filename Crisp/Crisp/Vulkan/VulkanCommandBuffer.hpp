#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;
    class VulkanCommandPool;

    class VulkanCommandBuffer
    {
    public:
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