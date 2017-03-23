#include "UniformMultiBuffer.hpp"

#include "VulkanDevice.hpp"
#include "VulkanRenderer.hpp"

namespace crisp
{
    UniformMultiBuffer::UniformMultiBuffer(VulkanDevice* device, VkDeviceSize initialSize, VkDeviceSize resSize, const void* data)
        : m_device(device)
        , m_resourceSize(resSize)
    {
        auto usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        auto deviceBufferHeap = m_device->getDeviceBufferHeap();

        for (uint32_t i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            std::tie(m_buffers[i], m_memoryChunks[i]) = m_device->createBuffer(deviceBufferHeap, initialSize, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            m_isUpdated[i] = true;
        }

        auto stagingUsageFlags  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        auto stagingMemoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        auto stagingHeap        = m_device->getStagingBufferHeap();

        std::tie(m_stagingBuffer, m_stagingMemoryChunk) = m_device->createBuffer(stagingHeap, initialSize, stagingUsageFlags, stagingMemoryProps);
    }

    UniformMultiBuffer::~UniformMultiBuffer()
    {
        //m_memoryChunk.memoryHeap->free(m_memoryChunk);
        //vkDestroyBuffer(m_device->getHandle(), m_buffer, nullptr);
        //
        //if (m_stagingBuffer != VK_NULL_HANDLE)
        //{
        //    m_stagingMemoryChunk.memoryHeap->free(m_stagingMemoryChunk);
        //    vkDestroyBuffer(m_device->getHandle(), m_stagingBuffer, nullptr);
        //    m_stagingBuffer = VK_NULL_HANDLE;
        //}
    }

    void UniformMultiBuffer::updateStagingBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        memcpy(static_cast<char*>(m_device->getStagingMemoryPtr()) + m_stagingMemoryChunk.offset, data, static_cast<size_t>(size));
    }

    void UniformMultiBuffer::updateDeviceBuffer(VkCommandBuffer& commandBuffer, uint32_t currentFrameIndex)
    {
        //auto size = m_singleDeviceRegionSize;
        //
        //VkBufferCopy copyRegion = {};
        //copyRegion.srcOffset = 0;
        //copyRegion.dstOffset = currentFrameIndex * size;
        //copyRegion.size = size;
        //vkCmdCopyBuffer(commandBuffer, m_stagingBuffer, m_buffer, 1, &copyRegion);
    }

    uint32_t UniformMultiBuffer::getDynamicOffset(uint32_t currentFrameIndex) const
    {
        return currentFrameIndex;// *m_singleDeviceRegionSize;
    }

    void UniformMultiBuffer::resize(VkDeviceSize newSize, VulkanRenderer* renderer)
    {
        auto bufferInfo = m_device->createBuffer(m_device->getStagingBufferHeap(), newSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        memcpy(static_cast<char*>(m_device->getStagingMemoryPtr()) + bufferInfo.second.offset,
            static_cast<char*>(m_device->getStagingMemoryPtr()) + m_stagingMemoryChunk.offset,
            m_stagingMemoryChunk.size);

        renderer->scheduleStagingBufferForRemoval(m_stagingBuffer, m_stagingMemoryChunk);

        m_stagingBuffer = bufferInfo.first;
        m_stagingMemoryChunk = bufferInfo.second;

        for (uint32_t i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            m_isUpdated[i] = false;
        }
    }

    void UniformMultiBuffer::resizeBuffer(uint32_t index, VkDeviceSize newSize)
    {

    }
}