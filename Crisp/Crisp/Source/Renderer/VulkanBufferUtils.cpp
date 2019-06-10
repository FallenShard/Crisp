#include "VulkanBufferUtils.hpp"

namespace crisp
{
    std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice* device, VkDeviceSize size, const void* data)
    {
        auto stagingBuffer = std::make_unique<VulkanBuffer>(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (data)
            stagingBuffer->updateFromHost(data, size, 0);
        return stagingBuffer;
    }

    std::unique_ptr<VulkanBuffer> createVertexBuffer(VulkanDevice* device, VkDeviceSize size)
    {
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        return std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    std::unique_ptr<VulkanBuffer> createIndexBuffer(VulkanDevice* device, VkDeviceSize size)
    {
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        return std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
}