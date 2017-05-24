#pragma once

#include "MemoryHeap.hpp"

namespace crisp
{
    class VulkanDevice;

    class VulkanBuffer
    {
    public:
        VulkanBuffer(VulkanDevice* device, size_t size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memProps);
        ~VulkanBuffer();

        VkBuffer getHandle() const;

        VkDeviceSize getSize() const;

        void updateFromHost(const void* srcData, VkDeviceSize size, VkDeviceSize offset = 0);
        void updateFromStaging(const VulkanBuffer& srcBuffer);
        void copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);

    private:
        VulkanDevice* m_device;

        VkBuffer m_buffer;
        MemoryChunk m_memoryChunk;

        VkDeviceSize m_size;
    };
}