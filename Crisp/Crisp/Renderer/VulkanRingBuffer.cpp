#include <Crisp/Renderer/VulkanRingBuffer.hpp>

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
namespace {

VkDeviceSize getMinAlignment(const VkBufferUsageFlagBits bufferType, const Renderer& renderer) {
    switch (bufferType) {
    case VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT:
        return renderer.getPhysicalDevice().getLimits().minUniformBufferOffsetAlignment;
    case VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:
        return renderer.getPhysicalDevice().getLimits().minStorageBufferOffsetAlignment;
    default:
        return 1;
    }
}
} // namespace

VulkanRingBuffer::VulkanRingBuffer(
    Renderer* renderer,
    VkBufferUsageFlagBits bufferType,
    size_t size,
    BufferUpdatePolicy updatePolicy,
    const void* data)
    : m_renderer(renderer)
    , m_bufferType(bufferType)
    , m_updatePolicy(updatePolicy)
    , m_size(size)
    , m_framesToUpdateOnGpu(RendererConfig::VirtualFrameCount) {
    const auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | m_bufferType;
    auto& device = m_renderer->getDevice();

    if (m_updatePolicy == BufferUpdatePolicy::Constant) {
        m_singleRegionSize = size;
        m_buffer = std::make_unique<VulkanBuffer>(device, m_size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            m_renderer->fillDeviceBuffer(m_buffer.get(), data, m_size);
        }
    } else if (m_updatePolicy == BufferUpdatePolicy::PerFrame) // Setup ring buffering
    {
        const VkDeviceSize minAlignment{getMinAlignment(m_bufferType, *renderer)};
        const VkDeviceSize unitsOfAlignment = ((m_size - 1) / minAlignment) + 1;
        m_singleRegionSize = unitsOfAlignment * minAlignment;
        m_buffer = std::make_unique<VulkanBuffer>(
            device,
            RendererConfig::VirtualFrameCount * m_singleRegionSize,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, m_singleRegionSize);

        if (data) {
            updateStagingBuffer(data, m_size);
        }

        // m_renderer->registerStreamingUniformBuffer(this);
    }
}

VulkanRingBuffer::~VulkanRingBuffer() {
    if (m_updatePolicy == BufferUpdatePolicy::PerFrame) {
        // m_renderer->unregisterStreamingUniformBuffer(this);
    }
}

void VulkanRingBuffer::updateStagingBuffer(const void* data, const VkDeviceSize size, const VkDeviceSize offset) {
    m_stagingBuffer->updateFromHost(data, size, offset);
    m_framesToUpdateOnGpu = RendererConfig::VirtualFrameCount;
}

void VulkanRingBuffer::updateDeviceBuffer(const VkCommandBuffer commandBuffer, const uint32_t currentFrameIndex) {
    if (m_framesToUpdateOnGpu == 0) {
        return;
    }

    m_buffer->copyFrom(
        commandBuffer, *m_stagingBuffer, 0, currentFrameIndex * m_singleRegionSize, m_stagingBuffer->getSize());

    m_framesToUpdateOnGpu--;
}

uint32_t VulkanRingBuffer::getRegionOffset(uint32_t regionIndex) const {
    return static_cast<uint32_t>(regionIndex * m_singleRegionSize);
}

VkDescriptorBufferInfo VulkanRingBuffer::getDescriptorInfo() const {
    return {m_buffer->getHandle(), 0, m_singleRegionSize};
}

VkDescriptorBufferInfo VulkanRingBuffer::getDescriptorInfo(const VkDeviceSize offset, const VkDeviceSize range) const {
    return {m_buffer->getHandle(), offset, range};
}

VkDescriptorBufferInfo VulkanRingBuffer::getDescriptorInfo(const uint32_t currentFrameIndex) const {
    return {m_buffer->getHandle(), getRegionOffset(currentFrameIndex), m_singleRegionSize};
}

} // namespace crisp