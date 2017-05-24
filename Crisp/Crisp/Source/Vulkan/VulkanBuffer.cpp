#include "VulkanBuffer.hpp"

#include <iostream>

#include "VulkanDevice.hpp"

namespace crisp
{
    VulkanBuffer::VulkanBuffer(VulkanDevice* device, size_t size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memProps)
        : m_device(device)
        , m_size(size)
    {
        // Create a buffer handle
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size        = size;
        bufferInfo.usage       = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(m_device->getHandle(), &bufferInfo, nullptr, &m_buffer);

        // Assign the buffer to a suitable memory heap by giving it a free chunk
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->getHandle(), m_buffer, &memRequirements);

        auto heap = m_device->getHeapFromMemProps(m_buffer, memProps, memRequirements.memoryTypeBits);
        m_memoryChunk = heap->allocateChunk(memRequirements.size, memRequirements.alignment);

        vkBindBufferMemory(m_device->getHandle(), m_buffer, heap->memory, m_memoryChunk.offset);
    }

    VulkanBuffer::~VulkanBuffer()
    {
        m_memoryChunk.free();
        vkDestroyBuffer(m_device->getHandle(), m_buffer, nullptr);
    }

    VkBuffer VulkanBuffer::getHandle() const
    {
        return m_buffer;
    }

    VkDeviceSize VulkanBuffer::getSize() const
    {
        return m_size;
    }

    void VulkanBuffer::updateFromHost(const void* srcData, VkDeviceSize size, VkDeviceSize offset)
    {
        memcpy(static_cast<char*>(m_device->getStagingMemoryPtr()) + m_memoryChunk.offset + offset, srcData, static_cast<size_t>(size));
        m_device->invalidateMappedRange(m_memoryChunk.memoryHeap->memory, m_memoryChunk.offset + offset, size);
    }

    void VulkanBuffer::updateFromStaging(const VulkanBuffer& srcBuffer)
    {
        memcpy(static_cast<char*>(m_device->getStagingMemoryPtr()) + m_memoryChunk.offset,
               static_cast<char*>(m_device->getStagingMemoryPtr()) + srcBuffer.m_memoryChunk.offset,
               srcBuffer.m_memoryChunk.size);
        m_device->invalidateMappedRange(m_memoryChunk.memoryHeap->memory, m_memoryChunk.offset, m_memoryChunk.size);
    }

    void VulkanBuffer::copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
    {
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size      = size;
        vkCmdCopyBuffer(cmdBuffer, srcBuffer.m_buffer, m_buffer, 1, &copyRegion);
    }
}
