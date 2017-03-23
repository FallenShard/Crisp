#pragma once

#include <array>

#include "MemoryHeap.hpp"
#include "VulkanRenderer.hpp"

namespace crisp
{
    class VulkanDevice;

    // Models resizable, frequently (per-frame) updated uniform buffer (for batching transforms etc.)
    class UniformMultiBuffer
    {
    public:
        UniformMultiBuffer(VulkanDevice* device, VkDeviceSize initialSize, VkDeviceSize resourceSize, const void* data = nullptr);
        ~UniformMultiBuffer();

        inline VkBuffer get(uint32_t index) const { return m_buffers[index]; }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

        uint32_t getDynamicOffset(uint32_t currentFrameIndex) const;

        void resize(VkDeviceSize newSize, VulkanRenderer* renderer);

    private:
        void resizeBuffer(uint32_t index, VkDeviceSize newSize);

        VulkanDevice* m_device;

        VkBuffer m_buffers[VulkanRenderer::NumVirtualFrames];
        MemoryChunk m_memoryChunks[VulkanRenderer::NumVirtualFrames];
        bool m_isUpdated[VulkanRenderer::NumVirtualFrames];

        VkBuffer m_stagingBuffer;
        MemoryChunk m_stagingMemoryChunk;

        VkDeviceSize m_resourceSize;
    };
}