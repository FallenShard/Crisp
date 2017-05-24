#include "UniformBuffer.hpp"

#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    UniformBuffer::UniformBuffer(VulkanRenderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data)
        : m_renderer(renderer)
        , m_updatePolicy(updatePolicy)
        , m_framesToUpdateOnGpu(VulkanRenderer::NumVirtualFrames)
    {
        auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        auto device = m_renderer->getDevice();

        if (m_updatePolicy == BufferUpdatePolicy::Constant)
        {
            m_buffer = std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (data != nullptr)
            {
                m_renderer->fillDeviceBuffer(m_buffer.get(), data, size);
            }

            m_singleRegionSize = size;
        }
        else if (m_updatePolicy == BufferUpdatePolicy::PerFrame) // Setup triple buffering
        {
            m_singleRegionSize = std::max(size, SizeGranularity);
            m_buffer = std::make_unique<VulkanBuffer>(device, VulkanRenderer::NumVirtualFrames * m_singleRegionSize, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            m_stagingBuffer = std::make_unique<VulkanBuffer>(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        }
    }

    UniformBuffer::~UniformBuffer()
    {
    }

    void UniformBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        m_stagingBuffer->updateFromHost(data, size, offset);
        m_framesToUpdateOnGpu = VulkanRenderer::NumVirtualFrames;
    }

    void UniformBuffer::updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex)
    {
        if (m_framesToUpdateOnGpu <= 0)
            return;

        m_buffer->copyFrom(commandBuffer, *m_stagingBuffer, 0, currentFrameIndex * m_singleRegionSize, m_stagingBuffer->getSize());

        m_framesToUpdateOnGpu--;
    }

    uint32_t UniformBuffer::getDynamicOffset(uint32_t currentFrameIndex) const
    {
        return static_cast<uint32_t>(currentFrameIndex * m_singleRegionSize);
    }

    VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo() const
    {
        return { m_buffer->getHandle(), 0, m_singleRegionSize };
    }

    VkDescriptorBufferInfo UniformBuffer::getDescriptorInfo(VkDeviceSize offset, VkDeviceSize range) const
    {
        return { m_buffer->getHandle(), offset, range };
    }
}