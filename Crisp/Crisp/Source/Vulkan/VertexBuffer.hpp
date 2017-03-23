#pragma once

#include <vector>

#include "MemoryHeap.hpp"

#include "UniformBuffer.hpp"

namespace crisp
{
    class VulkanDevice;

    class VertexBuffer
    {
    public:
        VertexBuffer(VulkanDevice* device, size_t size, BufferUpdatePolicy updatePolicy, const void* data = nullptr);

        template <typename T>
        VertexBuffer(VulkanDevice* device, const std::vector<T>& data, BufferUpdatePolicy updatePolicy = BufferUpdatePolicy::Constant)
            : VertexBuffer(device, data.size() * sizeof(T), updatePolicy, data.data()) {}

        ~VertexBuffer();

        inline VkBuffer get() const { return m_buffer; }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        
        template <typename T>
        void updateStagingBuffer(const std::vector<T>& vec)
        {
            updateStagingBuffer(vec.data(), vec.size() * sizeof(T));
        }

        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

    private:
        VulkanDevice* m_device;

        BufferUpdatePolicy m_updatePolicy;

        VkBuffer m_buffer;
        MemoryChunk m_memoryChunk;

        VkBuffer m_stagingBuffer;
        MemoryChunk m_stagingMemoryChunk;

        VkDeviceSize m_singleRegionSize;
    };
}
