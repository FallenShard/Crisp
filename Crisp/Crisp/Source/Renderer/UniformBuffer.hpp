#pragma once

#include <memory>

#include <CrispCore/Math/Headers.hpp>

#include "Vulkan/VulkanMemoryHeap.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "Renderer/BufferUpdatePolicy.hpp"

namespace crisp
{
    class Renderer;

    class UniformBuffer
    {
    public:
        UniformBuffer(Renderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data = nullptr);
        UniformBuffer(Renderer* renderer, size_t size, bool isShaderStorageBuffer, const void* data = nullptr);
        UniformBuffer(Renderer* renderer, size_t size, bool isShaderStorageBuffer, BufferUpdatePolicy updatePolicy, const void* data = nullptr);

        template <typename T>
        inline UniformBuffer(Renderer* renderer, T&& data)
            : UniformBuffer(renderer, sizeof(T), BufferUpdatePolicy::Constant, &data)
        {
        }

        template <typename T>
        inline UniformBuffer(Renderer* renderer, const T& data, BufferUpdatePolicy updatePolicy)
            : UniformBuffer(renderer, sizeof(T), updatePolicy, &data)
        {
        }

        ~UniformBuffer();

        inline VkBuffer get() const { return m_buffer->getHandle(); }

        template <typename T>
        void updateStagingBuffer(T&& data)
        {
            updateStagingBuffer(&data, sizeof(T), 0);
        }

        void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void updateDeviceBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrameIndex);

        uint32_t getDynamicOffset(uint32_t currentFrameIndex) const;
        VkDescriptorBufferInfo getDescriptorInfo() const;
        VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const;

    private:
        Renderer* m_renderer;

        BufferUpdatePolicy m_updatePolicy;
        VkDeviceSize m_singleRegionSize;
        std::unique_ptr<VulkanBuffer> m_buffer;
        std::unique_ptr<VulkanBuffer> m_stagingBuffer;

        uint32_t m_framesToUpdateOnGpu;
    };

    struct DynamicBufferView
    {
        const UniformBuffer* buffer;
        uint32_t subOffset;
    };
}