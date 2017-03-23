#include "VertexBuffer.hpp"

#include "VulkanDevice.hpp"
#include "VulkanRenderer.hpp"

namespace crisp
{
    VertexBuffer::VertexBuffer(VulkanDevice* device, size_t size, BufferUpdatePolicy updatePolicy, const void* data)
        : m_device(device)
        , m_updatePolicy(updatePolicy)
        , m_buffer(VK_NULL_HANDLE)
        , m_stagingBuffer(VK_NULL_HANDLE)
    {
        // Buffer will be used as a vertex source and transferred to from staging memory
        auto usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        auto deviceBufferHeap = m_device->getDeviceBufferHeap();

        if (m_updatePolicy == BufferUpdatePolicy::Constant)
        {
            std::tie(m_buffer, m_memoryChunk) = m_device->createBuffer(deviceBufferHeap, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            
            if (data != nullptr)
            {
                m_device->fillDeviceBuffer(m_buffer, data, 0, size);
            }
        }
        else if (m_updatePolicy == BufferUpdatePolicy::PerFrame)
        {
            std::tie(m_buffer, m_memoryChunk) = m_device->createBuffer(deviceBufferHeap, VulkanRenderer::NumVirtualFrames * size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            auto stagingUsageFlags  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            auto stagingMemoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            auto stagingHeap        = m_device->getStagingBufferHeap();

            std::tie(m_stagingBuffer, m_stagingMemoryChunk) = m_device->createBuffer(stagingHeap, size, stagingUsageFlags, stagingMemoryProps);
        }

        m_singleRegionSize = size;
    }

    VertexBuffer::~VertexBuffer()
    {
        m_memoryChunk.memoryHeap->free(m_memoryChunk);
        vkDestroyBuffer(m_device->getHandle(), m_buffer, nullptr);

        if (m_stagingBuffer != VK_NULL_HANDLE)
        {
            m_stagingMemoryChunk.memoryHeap->free(m_stagingMemoryChunk);
            vkDestroyBuffer(m_device->getHandle(), m_stagingBuffer, nullptr);
            m_stagingBuffer = VK_NULL_HANDLE;
        }
    }

    void VertexBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        memcpy(static_cast<char*>(m_device->getStagingMemoryPtr()) + m_stagingMemoryChunk.offset, data, static_cast<size_t>(size));
    }

    void VertexBuffer::updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex)
    {
        auto size = m_singleRegionSize;

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = currentFrameIndex * size;
        copyRegion.size      = size;
        vkCmdCopyBuffer(commandBuffer, m_stagingBuffer, m_buffer, 1, &copyRegion);
    }
}