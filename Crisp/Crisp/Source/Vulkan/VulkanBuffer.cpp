#include "VulkanBuffer.hpp"

#include <iostream>

#include "VulkanDevice.hpp"

namespace crisp
{
    VulkanBuffer::VulkanBuffer(VulkanDevice* device, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memProps)
        : VulkanResource(device)
        , m_size(size)
    {
        // Create a buffer handle
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size        = size;
        bufferInfo.usage       = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(m_device->getHandle(), &bufferInfo, nullptr, &m_handle);

        // Assign the buffer to a suitable memory heap by giving it a free chunk
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->getHandle(), m_handle, &memRequirements);

        auto heap = m_device->getHeapFromMemProps(memProps, memRequirements.memoryTypeBits);
        m_memoryChunk = heap->allocate(memRequirements.size, memRequirements.alignment);

        vkBindBufferMemory(m_device->getHandle(), m_handle, m_memoryChunk.getMemory(), m_memoryChunk.offset);
    }

    VulkanBuffer::~VulkanBuffer()
    {
        m_memoryChunk.free();
        vkDestroyBuffer(m_device->getHandle(), m_handle, nullptr);
    }

    VkDeviceSize VulkanBuffer::getSize() const
    {
        return m_size;
    }

    void VulkanBuffer::updateFromHost(const void* srcData, VkDeviceSize size, VkDeviceSize offset)
    {
        memcpy(m_memoryChunk.getMappedPtr() + offset, srcData, static_cast<size_t>(size));
        m_device->invalidateMappedRange(m_memoryChunk.getMemory(), m_memoryChunk.offset + offset, size);
    }

    void VulkanBuffer::updateFromHost(const void* srcData)
    {
        memcpy(m_memoryChunk.getMappedPtr(), srcData, m_size);
        m_device->invalidateMappedRange(m_memoryChunk.getMemory(), m_memoryChunk.offset, VK_WHOLE_SIZE);
    }

    void VulkanBuffer::updateFromStaging(const VulkanBuffer& srcBuffer)
    {
        memcpy(m_memoryChunk.getMappedPtr(), srcBuffer.m_memoryChunk.getMappedPtr(), srcBuffer.m_memoryChunk.size);
        m_device->invalidateMappedRange(m_memoryChunk.getMemory(), m_memoryChunk.offset, m_memoryChunk.size);
    }

    void VulkanBuffer::copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
    {
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size      = size;
        vkCmdCopyBuffer(cmdBuffer, srcBuffer.m_handle, m_handle, 1, &copyRegion);
    }
}
