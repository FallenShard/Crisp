#pragma once

#include <memory>

#include "Math/Headers.hpp"

#include "Vulkan/VulkanMemoryHeap.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "Renderer/BufferUpdatePolicy.hpp"

namespace crisp
{
    class VulkanRenderer;

    class UniformBuffer
    {
    public:
        static constexpr VkDeviceSize SizeGranularity = 256;

        UniformBuffer(VulkanRenderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data = nullptr);
        ~UniformBuffer();

        inline VkBuffer get() const { return m_buffer->getHandle(); }

        template <typename T>
        void updateStagingBuffer(T&& data)
        {
            updateStagingBuffer(&data, sizeof(T));
        }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

        uint32_t getDynamicOffset(uint32_t currentFrameIndex) const;
        VkDescriptorBufferInfo getDescriptorInfo() const;
        VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const;

    private:
        VulkanRenderer* m_renderer;

        BufferUpdatePolicy m_updatePolicy;
        VkDeviceSize m_singleRegionSize;
        std::unique_ptr<VulkanBuffer> m_buffer;
        std::unique_ptr<VulkanBuffer> m_stagingBuffer;

        uint32_t m_framesToUpdateOnGpu;
    };
}