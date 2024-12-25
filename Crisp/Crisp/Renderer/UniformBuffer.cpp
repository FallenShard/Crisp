#include <Crisp/Renderer/UniformBuffer.hpp>

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
UniformBuffer::UniformBuffer(Renderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data)
    : m_renderer(renderer)
    , m_updatePolicy(updatePolicy)
    , m_singleRegionSize(size) {
    auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto& device = m_renderer->getDevice();

    if (m_updatePolicy == BufferUpdatePolicy::Constant) {
        m_buffer = std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            fillDeviceBuffer(*m_renderer, m_buffer.get(), data, size);
        }

        m_singleRegionSize = size;
    } else if (m_updatePolicy == BufferUpdatePolicy::PerFrame) {
        m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, kRendererVirtualFrameCount * size);
        m_buffer = std::make_unique<VulkanBuffer>(
            device, kRendererVirtualFrameCount * m_singleRegionSize, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data) {
            updateStagingBuffer2(data, size, 0, renderer->getCurrentVirtualFrameIndex());
            renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer) {
                updateDeviceBuffer(commandBuffer);
            });
        }

        m_renderer->registerStreamingUniformBuffer(this);
    }
}

UniformBuffer::UniformBuffer(Renderer* renderer, size_t size, bool isShaderStorageBuffer, const void* data)
    : m_renderer(renderer)
    , m_updatePolicy(BufferUpdatePolicy::PerFrame)
    , m_singleRegionSize(0)
    , m_buffer(nullptr) {
    auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto& device = m_renderer->getDevice();

    if (isShaderStorageBuffer) {
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        m_singleRegionSize = std::max(size, renderer->getPhysicalDevice().getLimits().minStorageBufferOffsetAlignment);
        m_buffer = std::make_unique<VulkanBuffer>(
            device, kRendererVirtualFrameCount * m_singleRegionSize, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            fillDeviceBuffer(*m_renderer, m_buffer.get(), data, size);
        }
    }
}

UniformBuffer::UniformBuffer(
    Renderer* renderer, size_t size, bool isShaderStorageBuffer, BufferUpdatePolicy updatePolicy, const void* data)
    : m_renderer(renderer)
    , m_updatePolicy(updatePolicy)
    , m_singleRegionSize(0)
    , m_buffer(nullptr) {
    auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (isShaderStorageBuffer) {
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    auto& device = m_renderer->getDevice();

    if (m_updatePolicy == BufferUpdatePolicy::Constant) {
        m_buffer = std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (data != nullptr) {
            fillDeviceBuffer(*m_renderer, m_buffer.get(), data, size);
        }

        m_singleRegionSize = size;
    } else if (m_updatePolicy == BufferUpdatePolicy::PerFrame) // Setup ring buffering
    {
        m_singleRegionSize = std::max(size, renderer->getPhysicalDevice().getLimits().minUniformBufferOffsetAlignment);
        m_buffer = std::make_unique<VulkanBuffer>(
            device, kRendererVirtualFrameCount * m_singleRegionSize, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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

void UniformBuffer::updateStagingBuffer2(
    const void* data, const VkDeviceSize size, const VkDeviceSize offset, const uint32_t regionIndex) {
    m_stagingBuffer->updateFromHost(data, size, offset);

    m_hasUpdate = true;
    m_lastUpdatedRegion = regionIndex;
}

void UniformBuffer::updateDeviceBuffer(const VkCommandBuffer commandBuffer) {
    if (!m_hasUpdate) {
        return;
    }

    m_buffer->copyFrom(commandBuffer, *m_stagingBuffer, m_lastUpdatedRegion * m_singleRegionSize, 0, m_singleRegionSize);
    m_hasUpdate = false;
}

VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo() const {
    return {m_buffer->getHandle(), 0, m_singleRegionSize};
}

VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const {
    return {m_buffer->getHandle(), offset, range};
}

} // namespace crisp