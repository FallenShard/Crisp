#pragma once

#include "MemoryHeap.hpp"

namespace crisp
{
    class VulkanDevice;

    enum class BufferUpdatePolicy
    {
        Constant,
        PerFrame
    };

    class UniformBuffer
    {
    public:
        UniformBuffer(VulkanDevice* device, VkDeviceSize size, BufferUpdatePolicy updatePolicy, const void* data = nullptr);
        ~UniformBuffer();

        inline VkBuffer get() const { return m_buffer; }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

        uint32_t getDynamicOffset(uint32_t currentFrameIndex) const;

    private:
        VulkanDevice* m_device;

        VkBuffer m_buffer;
        MemoryChunk m_memoryChunk;

        BufferUpdatePolicy m_updatePolicy;

        VkBuffer m_stagingBuffer;
        MemoryChunk m_stagingMemoryChunk;

        VkDeviceSize m_deviceRegionSize;
        uint32_t m_framesToUpdateOnGpu;
    };
}