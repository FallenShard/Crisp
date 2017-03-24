#pragma once

#include <vulkan/vulkan.h>
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanRenderer.hpp"

namespace crisp
{
    namespace experiment
    {
        struct ConstantUpdatePolicy
        {
            VkBuffer buffer;
            MemoryChunk memoryChunk;
        };

        struct PerFrameUpdatePolicy
        {
            VkBuffer buffer;
            MemoryChunk memoryChunk;
        };

        struct MutableUpdatePolicy
        {
            VkBuffer buffers[VulkanRenderer::NumVirtualFrames];
            MemoryChunk memoryChunks[VulkanRenderer::NumVirtualFrames];
        };

        enum class BufferUsage
        {
            Vertex,
            Index,
            Uniform
        };

        namespace internal
        {
            template <BufferUsage usage>
            struct BufferUsageTraits
            {
                static constexpr VkBufferUsageFlags usageFlag = 0;
            };

            template <>
            struct BufferUsageTraits<BufferUsage::Vertex>
            {
                static constexpr VkBufferUsageFlags usageFlag = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            };

            template <>
            struct BufferUsageTraits<BufferUsage::Index>
            {
                static constexpr VkBufferUsageFlags usageFlag = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            };

            template <>
            struct BufferUsageTraits<BufferUsage::Uniform>
            {
                static constexpr VkBufferUsageFlags usageFlag = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            };
        }

        template <BufferUsage usage, typename UpdatePolicy>
        class DeviceBuffer
        {
        public:
            DeviceBuffer()
            {
                // TBC...
            }

        private:
            UpdatePolicy m_updatePolicy;

        };
    }
}