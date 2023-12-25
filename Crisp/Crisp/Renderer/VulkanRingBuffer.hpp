#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/BufferUpdatePolicy.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>

#include <memory>

namespace crisp {
class Renderer;

namespace internal {
template <typename T>
struct IndexTypeTrait;

template <>
struct IndexTypeTrait<glm::u16vec2> {
    static constexpr VkIndexType value = VK_INDEX_TYPE_UINT16;
};

template <>
struct IndexTypeTrait<glm::u16vec3> {
    static constexpr VkIndexType value = VK_INDEX_TYPE_UINT16;
};

template <>
struct IndexTypeTrait<glm::uvec3> {
    static constexpr VkIndexType value = VK_INDEX_TYPE_UINT32;
};

template <>
struct IndexTypeTrait<glm::uvec4> {
    static constexpr VkIndexType value = VK_INDEX_TYPE_UINT32;
};
} // namespace internal

class VulkanRingBuffer {
public:
    VulkanRingBuffer(
        Renderer* renderer,
        VkBufferUsageFlagBits bufferType,
        size_t size,
        BufferUpdatePolicy updatePolicy,
        const void* data = nullptr);

    VulkanRingBuffer(const VulkanRingBuffer&) = delete;
    VulkanRingBuffer& operator=(const VulkanRingBuffer&) = delete;

    VulkanRingBuffer(VulkanRingBuffer&&) noexcept = default;
    VulkanRingBuffer& operator=(VulkanRingBuffer&&) noexcept = default;

    ~VulkanRingBuffer();

    inline VkBuffer getHandle() const {
        return m_buffer->getHandle();
    }

    void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    template <typename T>
    void updateStagingBufferFromStdVec(const std::vector<T>& data) {
        updateStagingBuffer(data.data(), sizeof(T) * data.size(), 0);
    }

    void updateDeviceBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrameIndex);

    uint32_t getRegionOffset(uint32_t regionIndex) const;
    VkDescriptorBufferInfo getDescriptorInfo() const;
    VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const;
    VkDescriptorBufferInfo getDescriptorInfo(uint32_t currentFrameIndex) const;

private:
    Renderer* m_renderer;

    VkBufferUsageFlagBits m_bufferType;

    BufferUpdatePolicy m_updatePolicy;
    VkDeviceSize m_size;
    VkDeviceSize m_singleRegionSize;
    std::unique_ptr<VulkanBuffer> m_buffer;
    std::unique_ptr<StagingVulkanBuffer> m_stagingBuffer;

    uint32_t m_framesToUpdateOnGpu;
};
} // namespace crisp
