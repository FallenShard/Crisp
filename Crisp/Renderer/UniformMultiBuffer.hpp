#pragma once

#include <vector>
#include <memory>

#include "Vulkan/VulkanMemoryHeap.hpp"
#include "Vulkan/VulkanBuffer.hpp"

namespace crisp
{
    class Renderer;

    // Models resizable, frequently (per-frame) updated uniform buffer (for batching transforms etc.)
    class UniformMultiBuffer
    {
    public:
        UniformMultiBuffer(Renderer* renderer, VkDeviceSize initialSize, VkDeviceSize resourceSize, const void* data = nullptr);
        ~UniformMultiBuffer();

        inline VkBuffer get(uint32_t index) const { return m_buffers[index]->getHandle(); }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

        uint32_t getDynamicOffset(uint32_t currentFrameIndex) const;

        void resize(VkDeviceSize newSize);
        void resizeOnDevice(uint32_t index);

    private:
        Renderer* m_renderer;

        std::vector<std::unique_ptr<VulkanBuffer>> m_buffers;
        std::unique_ptr<VulkanBuffer>              m_stagingBuffer;

        VkDeviceSize m_singleRegionSize;
    };
}