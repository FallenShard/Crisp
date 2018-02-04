#include "IndexBuffer.hpp"

#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    IndexBuffer::IndexBuffer(VulkanRenderer* renderer, VkIndexType indexType, BufferUpdatePolicy updatePolicy, size_t size, const void* data)
        : m_renderer(renderer)
        , m_indexType(indexType)
        , m_updatePolicy(updatePolicy)
    {
        // Buffer will be used as a index source and transferred to from staging memory
        auto usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        auto device     = m_renderer->getDevice();

        if (m_updatePolicy == BufferUpdatePolicy::Constant)
        {
            m_buffer = std::make_unique<VulkanBuffer>(device, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (data != nullptr)
            {
                m_renderer->fillDeviceBuffer(m_buffer.get(), data, size);
            }
        }
        else if (m_updatePolicy == BufferUpdatePolicy::PerFrame)
        {
            m_buffer = std::make_unique<VulkanBuffer>(device, VulkanRenderer::NumVirtualFrames * size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            auto stagingUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            auto stagingMemoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

            m_stagingBuffer = std::make_unique<VulkanBuffer>(device, size, stagingUsageFlags, stagingMemoryProps);
        }

        m_singleRegionSize = size;
    }

    IndexBuffer::~IndexBuffer()
    {
    }

    void IndexBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        m_stagingBuffer->updateFromHost(data, size, offset);
    }

    void IndexBuffer::updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex)
    {
        m_buffer->copyFrom(commandBuffer, *m_stagingBuffer, 0, currentFrameIndex * m_singleRegionSize, m_singleRegionSize);
    }

    void IndexBuffer::bind(VkCommandBuffer& commandBuffer, VkDeviceSize offset)
    {
        vkCmdBindIndexBuffer(commandBuffer, m_buffer->getHandle(), offset, m_indexType);
    }
}