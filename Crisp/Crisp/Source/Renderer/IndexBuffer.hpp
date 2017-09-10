#pragma once

#include <vector>
#include <memory>

#include "Math/Headers.hpp"

#include "Vulkan/MemoryHeap.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "Renderer/BufferUpdatePolicy.hpp"

namespace crisp
{
    class VulkanRenderer;

    namespace internal
    {
        template <typename T> struct IndexTypeTrait               { static constexpr VkIndexType value = VK_INDEX_TYPE_UINT32; };
        template <>           struct IndexTypeTrait<glm::u16vec3> { static constexpr VkIndexType value = VK_INDEX_TYPE_UINT16; };
        template <>           struct IndexTypeTrait<glm::uvec3>   { static constexpr VkIndexType value = VK_INDEX_TYPE_UINT32; };
    }

    class IndexBuffer
    {
    public:
        IndexBuffer(VulkanRenderer* renderer, VkIndexType indexType, BufferUpdatePolicy updatePolicy, size_t size, const void* data = nullptr);

        template <typename T>
        IndexBuffer(VulkanRenderer* renderer, const std::vector<T>& data, BufferUpdatePolicy updatePolicy = BufferUpdatePolicy::Constant)
            : IndexBuffer(renderer, internal::IndexTypeTrait<T>::value, updatePolicy, data.size() * sizeof(T), data.data()) {}

        ~IndexBuffer();

        inline VkBuffer get() const { return m_buffer->getHandle(); }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        template <typename T>
        void updateStagingBuffer(const std::vector<T>& vec)
        {
            updateStagingBuffer(vec.data(), vec.size() * sizeof(T));
        }

        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

        void bind(VkCommandBuffer& commandBuffer, VkDeviceSize offset);

    private:
        VulkanRenderer* m_renderer;

        BufferUpdatePolicy m_updatePolicy;
        VkDeviceSize m_singleRegionSize;
        std::unique_ptr<VulkanBuffer> m_buffer;
        std::unique_ptr<VulkanBuffer> m_stagingBuffer;

        VkIndexType m_indexType;
    };
}