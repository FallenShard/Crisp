#include "UniformBuffer.hpp"

#include "VulkanDevice.hpp"
#include "VulkanRenderer.hpp"

namespace crisp
{
    UniformBuffer::UniformBuffer(VulkanDevice* device, VkDeviceSize size, BufferUpdatePolicy updatePolicy, const void* data)
        : m_device(device)
        , m_updatePolicy(updatePolicy)
    {
        auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        auto deviceBufferHeap = m_device->getDeviceBufferHeap();

        if (m_updatePolicy == BufferUpdatePolicy::Constant)
        {
            std::tie(m_buffer, m_memoryChunk) = m_device->createBuffer(deviceBufferHeap, size, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (data != nullptr)
            {
                m_device->fillDeviceBuffer(m_buffer, data, 0, size);
            }

            m_deviceRegionSize = size;
        }
        else if (m_updatePolicy == BufferUpdatePolicy::PerFrame) // Setup triple buffering
        {
            auto deviceRegionSize = std::max(size, 256ull);
            std::tie(m_buffer, m_memoryChunk) = m_device->createBuffer(deviceBufferHeap, VulkanRenderer::NumVirtualFrames * deviceRegionSize, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


            auto stagingUsageFlags  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            auto stagingMemoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            auto stagingHeap        = m_device->getStagingBufferHeap();

            std::tie(m_stagingBuffer, m_stagingMemoryChunk) = m_device->createBuffer(stagingHeap, size, stagingUsageFlags, stagingMemoryProps);

            m_deviceRegionSize = deviceRegionSize;
        }
    }

    UniformBuffer::~UniformBuffer()
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

    void UniformBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        memcpy(static_cast<char*>(m_device->getStagingMemoryPtr()) + m_stagingMemoryChunk.offset, data, static_cast<size_t>(size));
    }

    void UniformBuffer::updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex)
    {
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = currentFrameIndex * m_deviceRegionSize;
        copyRegion.size      = m_stagingMemoryChunk.size;
        vkCmdCopyBuffer(commandBuffer, m_stagingBuffer, m_buffer, 1, &copyRegion);
    }

    uint32_t UniformBuffer::getDynamicOffset(uint32_t currentFrameIndex) const
    {
        return currentFrameIndex * m_deviceRegionSize;
    }
}