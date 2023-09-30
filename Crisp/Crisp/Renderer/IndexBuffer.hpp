#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/BufferUpdatePolicy.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>

#include <memory>
#include <vector>

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

class IndexBuffer {
public:
    IndexBuffer(
        Renderer* renderer,
        VkIndexType indexType,
        BufferUpdatePolicy updatePolicy,
        size_t size,
        const void* data = nullptr);

    template <typename T>
    IndexBuffer(
        Renderer* renderer, const std::vector<T>& data, BufferUpdatePolicy updatePolicy = BufferUpdatePolicy::Constant)
        : IndexBuffer(
              renderer, internal::IndexTypeTrait<T>::value, updatePolicy, data.size() * sizeof(T), data.data()) {}

    ~IndexBuffer();

    inline VkBuffer get() const {
        return m_buffer->getHandle();
    }

    void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    template <typename T>
    void updateStagingBuffer(const std::vector<T>& vec) {
        updateStagingBuffer(vec.data(), vec.size() * sizeof(T));
    }

    void updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex);

    void bind(VkCommandBuffer& commandBuffer, VkDeviceSize offset);

private:
    Renderer* m_renderer;

    BufferUpdatePolicy m_updatePolicy;
    VkDeviceSize m_singleRegionSize;
    std::unique_ptr<VulkanBuffer> m_buffer;
    std::unique_ptr<StagingVulkanBuffer> m_stagingBuffer;

    VkIndexType m_indexType;
};
} // namespace crisp
