#include "VertexBuffer.hpp"

#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    VertexBuffer::VertexBuffer(VulkanRenderer* renderer, size_t size, BufferUpdatePolicy updatePolicy, const void* data)
        : m_renderer(renderer)
        , m_updatePolicy(updatePolicy)
    {
        // Buffer will be used as a vertex source and transferred to from staging memory
        auto usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
            m_stagingBuffer = std::make_unique<VulkanBuffer>(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        }

        m_singleRegionSize = size;
    }

    VertexBuffer::~VertexBuffer()
    {
    }

    void VertexBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        m_stagingBuffer->updateFromHost(data, size, offset);
    }

    void VertexBuffer::updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex)
    {
        m_buffer->copyFrom(commandBuffer, *m_stagingBuffer, 0, currentFrameIndex * m_singleRegionSize, m_singleRegionSize);
    }
}