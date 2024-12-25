#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/BufferUpdatePolicy.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanMemoryHeap.hpp>

namespace crisp {
class UniformBuffer {
public:
    UniformBuffer(Renderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data = nullptr);
    UniformBuffer(Renderer* renderer, size_t size, bool isShaderStorageBuffer, const void* data = nullptr);
    UniformBuffer(
        Renderer* renderer,
        size_t size,
        bool isShaderStorageBuffer,
        BufferUpdatePolicy updatePolicy,
        const void* data = nullptr);

    template <typename T>
    inline UniformBuffer(Renderer* renderer, T&& data)
        : UniformBuffer(renderer, sizeof(T), BufferUpdatePolicy::Constant, &data) {}

    template <typename T>
    inline UniformBuffer(Renderer* renderer, const T& data, BufferUpdatePolicy updatePolicy)
        : UniformBuffer(renderer, sizeof(T), updatePolicy, &data) {}

    ~UniformBuffer();

    inline VkBuffer get() const {
        return m_buffer->getHandle();
    }

    template <typename T>
    void updateStagingBuffer2(const T& data, const uint32_t regionIndex) {
        using BaseType = typename std::remove_cvref_t<T>;
        updateStagingBuffer2(&data, sizeof(BaseType), 0, regionIndex);
    }

    void updateStagingBuffer2(const void* data, VkDeviceSize size, VkDeviceSize offset, uint32_t regionIndex);
    void updateDeviceBuffer(VkCommandBuffer commandBuffer);

    VkDescriptorBufferInfo getDescriptorInfo() const;
    VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const;

private:
    Renderer* m_renderer;

    BufferUpdatePolicy m_updatePolicy;
    VkDeviceSize m_singleRegionSize;
    std::unique_ptr<VulkanBuffer> m_buffer;
    std::unique_ptr<StagingVulkanBuffer> m_stagingBuffer;

    bool m_hasUpdate{false};
    uint32_t m_lastUpdatedRegion{~0u};
};

struct DynamicBufferView {
    const UniformBuffer* buffer;
    uint32_t subOffset;
};
} // namespace crisp