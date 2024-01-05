#include <Crisp/Renderer/StorageBuffer.hpp>

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
StorageBuffer::StorageBuffer(
    Renderer* renderer,
    VkDeviceSize size,
    VkBufferUsageFlags additionalUsageFlags,
    BufferUpdatePolicy updatePolicy,
    const void* data)
    : m_renderer(renderer)
    , m_updatePolicy(updatePolicy)
    , m_singleRegionSize(0)
    , m_framesToUpdateOnGpu(kRendererVirtualFrameCount) {
    const VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | additionalUsageFlags | (data ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);

    if (m_updatePolicy == BufferUpdatePolicy::Constant) {
        m_buffer = std::make_unique<VulkanBuffer>(
            m_renderer->getDevice(), size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            fillDeviceBuffer(*m_renderer, m_buffer.get(), data, size);
        }

        m_singleRegionSize = size;
    } else if (m_updatePolicy == BufferUpdatePolicy::PerFrame || m_updatePolicy == BufferUpdatePolicy::PerFrameGpu) {
        m_singleRegionSize = std::max(size, renderer->getPhysicalDevice().getLimits().minStorageBufferOffsetAlignment);
        m_buffer = std::make_unique<VulkanBuffer>(
            m_renderer->getDevice(),
            kRendererVirtualFrameCount * m_singleRegionSize,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (m_updatePolicy == BufferUpdatePolicy::PerFrame) {
            m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(m_renderer->getDevice(), size);
            if (data) {
                updateStagingBuffer(data, size);
            }
            m_renderer->registerStreamingStorageBuffer(this);
        }
    }
}

StorageBuffer::~StorageBuffer() {
    if (m_updatePolicy == BufferUpdatePolicy::PerFrame || m_updatePolicy == BufferUpdatePolicy::PerFrameGpu) {
        m_renderer->unregisterStreamingStorageBuffer(this);
    }
}

void StorageBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    m_stagingBuffer->updateFromHost(data, size, offset);
    m_framesToUpdateOnGpu = kRendererVirtualFrameCount;
}

void StorageBuffer::updateDeviceBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrameIndex) {
    if (m_framesToUpdateOnGpu == 0) {
        return;
    }

    m_buffer->copyFrom(
        commandBuffer, *m_stagingBuffer, 0, currentFrameIndex * m_singleRegionSize, m_stagingBuffer->getSize());

    m_framesToUpdateOnGpu--;
}

uint32_t StorageBuffer::getDynamicOffset(uint32_t currentFrameIndex) const {
    return static_cast<uint32_t>(currentFrameIndex * m_singleRegionSize);
}

VkDescriptorBufferInfo StorageBuffer::getDescriptorInfo() const {
    return {m_buffer->getHandle(), 0, m_singleRegionSize};
}

VkDescriptorBufferInfo StorageBuffer::getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const {
    return {m_buffer->getHandle(), offset, range};
}

VkDescriptorBufferInfo StorageBuffer::createDescriptorInfoFromSection(uint32_t sectionIndex) {
    return {m_buffer->getHandle(), sectionIndex * m_singleRegionSize, m_singleRegionSize};
}
} // namespace crisp