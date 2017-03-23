#pragma once

#include <vector>

#include "Math/Headers.hpp"

#include "MemoryHeap.hpp"
#include "UniformBuffer.hpp"

namespace crisp
{
    class VulkanDevice;

    namespace internal
    {
        template <typename T>
        struct IndexTypeTrait
        {
            static constexpr VkIndexType value = VK_INDEX_TYPE_UINT32;
        };

        template <>
        struct IndexTypeTrait<glm::u16vec3>
        {
            static constexpr VkIndexType value = VK_INDEX_TYPE_UINT16;
        };

        template <>
        struct IndexTypeTrait<glm::uvec3>
        {
            static constexpr VkIndexType value = VK_INDEX_TYPE_UINT32;
        };
    }

    class IndexBuffer
    {
    public:
        IndexBuffer(VulkanDevice* device, VkIndexType indexType, BufferUpdatePolicy updatePolicy, size_t size, const void* data = nullptr);

        template <typename T>
        IndexBuffer(VulkanDevice* device, const std::vector<T>& data, BufferUpdatePolicy updatePolicy = BufferUpdatePolicy::Constant)
            : IndexBuffer(device, internal::IndexTypeTrait<T>::value, updatePolicy, data.size() * sizeof(T), data.data())
        {
        }

        ~IndexBuffer();

        inline VkBuffer get() const { return m_buffer; }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        template <typename T>
        void updateStagingBuffer(const std::vector<T>& vec)
        {
            updateStagingBuffer(vec.data(), vec.size() * sizeof(T));
        }

        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

        void bind(VkCommandBuffer& commandBuffer, VkDeviceSize offset);

    private:
        VulkanDevice* m_device;

        VkBuffer m_buffer;
        MemoryChunk m_memoryChunk;

        VkBuffer m_stagingBuffer;
        MemoryChunk m_stagingMemoryChunk;

        VkIndexType m_indexType;
        BufferUpdatePolicy m_updatePolicy;

        VkDeviceSize m_singleRegionSize;
    };
}
