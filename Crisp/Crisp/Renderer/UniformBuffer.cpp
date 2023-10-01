#include "UniformBuffer.hpp"

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
UniformBuffer::UniformBuffer(Renderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data)
    : m_renderer(renderer)
    , m_updatePolicy(updatePolicy)
    , m_framesToUpdateOnGpu(RendererConfig::VirtualFrameCount)
    , m_singleRegionSize(0)
    , m_buffer(nullptr) {
    auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto& device = m_renderer->getDevice();

    if (m_updatePolicy == BufferUpdatePolicy::Constant) {
        m_buffer = std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            m_renderer->fillDeviceBuffer(m_buffer.get(), data, size);
        }

        m_singleRegionSize = size;
    } else if (m_updatePolicy == BufferUpdatePolicy::PerFrame) // Setup ring buffering
    {
        const VkDeviceSize minAlignment = renderer->getPhysicalDevice().getLimits().minUniformBufferOffsetAlignment;
        const VkDeviceSize unitsOfAlignment = ((size - 1) / minAlignment) + 1;
        m_singleRegionSize = unitsOfAlignment * minAlignment;
        m_buffer = std::make_unique<VulkanBuffer>(
            device,
            RendererConfig::VirtualFrameCount * m_singleRegionSize,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, m_singleRegionSize);

        if (data) {
            updateStagingBuffer(data, size);
        }

        m_renderer->registerStreamingUniformBuffer(this);
    }
}

UniformBuffer::UniformBuffer(Renderer* renderer, size_t size, bool isShaderStorageBuffer, const void* data)
    : m_renderer(renderer)
    , m_updatePolicy(BufferUpdatePolicy::PerFrame)
    , m_singleRegionSize(0)
    , m_buffer(nullptr)
    , m_framesToUpdateOnGpu(RendererConfig::VirtualFrameCount) {
    auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto& device = m_renderer->getDevice();

    if (isShaderStorageBuffer) {
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        m_singleRegionSize = std::max(size, renderer->getPhysicalDevice().getLimits().minStorageBufferOffsetAlignment);
        m_buffer = std::make_unique<VulkanBuffer>(
            device,
            RendererConfig::VirtualFrameCount * m_singleRegionSize,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            m_renderer->fillDeviceBuffer(m_buffer.get(), data, size);
        }
    }
}

UniformBuffer::UniformBuffer(
    Renderer* renderer, size_t size, bool isShaderStorageBuffer, BufferUpdatePolicy updatePolicy, const void* data)
    : m_renderer(renderer)
    , m_updatePolicy(updatePolicy)
    , m_singleRegionSize(0)
    , m_buffer(nullptr)
    , m_framesToUpdateOnGpu(RendererConfig::VirtualFrameCount) {
    auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (isShaderStorageBuffer) {
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    auto& device = m_renderer->getDevice();

    if (m_updatePolicy == BufferUpdatePolicy::Constant) {
        m_buffer = std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            m_renderer->fillDeviceBuffer(m_buffer.get(), data, size);
        }

        m_singleRegionSize = size;
    } else if (m_updatePolicy == BufferUpdatePolicy::PerFrame) // Setup ring buffering
    {
        m_singleRegionSize = std::max(size, renderer->getPhysicalDevice().getLimits().minUniformBufferOffsetAlignment);
        m_buffer = std::make_unique<VulkanBuffer>(
            device,
            RendererConfig::VirtualFrameCount * m_singleRegionSize,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, size);

        // if (data)
        //     updateStagingBuffer(data, size);

        m_renderer->registerStreamingUniformBuffer(this);
    }
}

UniformBuffer::~UniformBuffer() {
    if (m_updatePolicy == BufferUpdatePolicy::PerFrame) {
        m_renderer->unregisterStreamingUniformBuffer(this);
    }
}

void UniformBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    m_stagingBuffer->updateFromHost(data, size, offset);
    m_framesToUpdateOnGpu = RendererConfig::VirtualFrameCount;
}

void UniformBuffer::updateDeviceBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrameIndex) {
    if (m_framesToUpdateOnGpu == 0) {
        return;
    }

    m_buffer->copyFrom(
        commandBuffer, *m_stagingBuffer, 0, currentFrameIndex * m_singleRegionSize, m_stagingBuffer->getSize());

    m_framesToUpdateOnGpu--;
}

uint32_t UniformBuffer::getDynamicOffset(uint32_t currentFrameIndex) const {
    return static_cast<uint32_t>(currentFrameIndex * m_singleRegionSize);
}

VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo() const {
    return {m_buffer->getHandle(), 0, m_singleRegionSize};
}

VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const {
    return {m_buffer->getHandle(), offset, range};
}

VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo(uint32_t currentFrameIndex) const {
    return {m_buffer->getHandle(), getDynamicOffset(currentFrameIndex), m_singleRegionSize};
}

} // namespace crisp