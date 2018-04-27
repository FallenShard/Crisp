#pragma once

#include <vector>
#include <memory>

#include "Vulkan/VulkanMemoryHeap.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "Renderer/BufferUpdatePolicy.hpp"

namespace crisp
{
    class VulkanRenderer;

    class VertexBuffer
    {
    public:
        VertexBuffer(VulkanRenderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data = nullptr);

        template <typename T>
        VertexBuffer(VulkanRenderer* renderer, const std::vector<T>& data, BufferUpdatePolicy updatePolicy = BufferUpdatePolicy::Constant)
            : VertexBuffer(renderer, data.size() * sizeof(T), updatePolicy, data.data()) {}

        ~VertexBuffer();

        inline VkBuffer get() const { return m_buffer->getHandle(); }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        template <typename T>
        void updateStagingBuffer(const std::vector<T>& vec)
        {
            updateStagingBuffer(vec.data(), vec.size() * sizeof(T));
        }

        template <typename T, size_t N>
        void updateStagingBuffer(const std::array<T, N>& array)
        {
            updateStagingBuffer(array.data(), N * sizeof(T));
        }

        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

    private:
        VulkanRenderer* m_renderer;

        BufferUpdatePolicy            m_updatePolicy;
        VkDeviceSize                  m_singleRegionSize;
        std::unique_ptr<VulkanBuffer> m_buffer;
        std::unique_ptr<VulkanBuffer> m_stagingBuffer;
    };
}
