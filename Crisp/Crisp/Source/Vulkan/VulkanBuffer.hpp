#pragma once

#include <vector>

#include "VulkanMemoryHeap.hpp"
#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanDevice;

    class VulkanBuffer : public VulkanResource<VkBuffer>
    {
    public:
        VulkanBuffer(VulkanDevice* device, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memProps);
        ~VulkanBuffer();

        VkDeviceSize getSize() const;

        void updateFromHost(const void* srcData, VkDeviceSize size, VkDeviceSize offset);
        void updateFromHost(const void* srcData);

        template <typename T>
        void updateFromHost(const std::vector<T>& buffer)
        {
            updateFromHost(buffer.data(), buffer.size() * sizeof(T), 0);
        }

        void updateFromStaging(const VulkanBuffer& srcBuffer);
        void copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);

    private:
        VulkanMemoryChunk m_memoryChunk;
        VkDeviceSize      m_size;
    };
}