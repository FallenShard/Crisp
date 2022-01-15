#pragma once

#include <memory>

#include <CrispCore/Math/Headers.hpp>

#include <Crisp/Renderer/BufferUpdatePolicy.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>

namespace crisp
{
class Renderer;

class StorageBuffer
{
public:
    StorageBuffer(Renderer* renderer, VkDeviceSize size, VkBufferUsageFlags additionalUsageFlags = 0,
        BufferUpdatePolicy updatePolicy = BufferUpdatePolicy::PerFrameGpu, const void* data = nullptr);

    ~StorageBuffer();

    inline VkBuffer getHandle() const
    {
        return m_buffer->getHandle();
    }

    inline VulkanBuffer* getDeviceBuffer() const
    {
        return m_buffer.get();
    }

    template <typename T>
    void updateStagingBuffer(T&& data)
    {
        using BaseType = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
        updateStagingBuffer(&data, sizeof(BaseType), 0);
    }

    void updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void updateDeviceBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrameIndex);

    uint32_t getDynamicOffset(uint32_t currentFrameIndex) const;
    VkDescriptorBufferInfo getDescriptorInfo() const;
    VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const;

    VulkanBufferSpan createSpanFromSection(uint32_t sectionIndex);

private:
    Renderer* m_renderer;

    BufferUpdatePolicy m_updatePolicy;
    VkDeviceSize m_singleRegionSize;
    std::unique_ptr<VulkanBuffer> m_buffer;
    std::unique_ptr<StagingVulkanBuffer> m_stagingBuffer;

    uint32_t m_framesToUpdateOnGpu;
};
} // namespace crisp