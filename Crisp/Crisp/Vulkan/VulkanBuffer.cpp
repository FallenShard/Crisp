#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp
{
VulkanBuffer::VulkanBuffer(const VulkanDevice& device, const VkDeviceSize size, const VkBufferUsageFlags usageFlags,
    const VkMemoryPropertyFlags memProps)
    : VulkanResource(device.getResourceDeallocator())
    , m_size(size)
{
    // Create a buffer handle
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    m_handle = device.createBuffer(bufferInfo);

    // Allocate the required memory
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.getHandle(), m_handle, &memRequirements);
    m_allocation = device.getMemoryAllocator().allocate(memProps, memRequirements).unwrap();
    vkBindBufferMemory(device.getHandle(), m_handle, m_allocation.getMemory(), m_allocation.offset);
}

VulkanBuffer::~VulkanBuffer()
{
    m_deallocator->deferMemoryDeallocation(m_framesToLive, m_allocation);
}

VkDeviceSize VulkanBuffer::getSize() const
{
    return m_size;
}

void VulkanBuffer::copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer)
{
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = m_size;
    vkCmdCopyBuffer(cmdBuffer, srcBuffer.m_handle, m_handle, 1, &copyRegion);
}

void VulkanBuffer::copyFrom(const VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer,
    const VkDeviceSize srcOffset, const VkDeviceSize dstOffset, const VkDeviceSize size)
{
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmdBuffer, srcBuffer.m_handle, m_handle, 1, &copyRegion);
}

VulkanBufferSpan VulkanBuffer::createSpan() const
{
    return { m_handle, 0, m_size };
}

StagingVulkanBuffer::StagingVulkanBuffer(VulkanDevice& device, const VkDeviceSize size,
    const VkBufferUsageFlags usageFlags, const VkMemoryPropertyFlags memProps)
    : VulkanBuffer(device, size, usageFlags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          memProps | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    , m_device(&device)
{
}

void StagingVulkanBuffer::updateFromHost(const void* srcData, const VkDeviceSize size, const VkDeviceSize offset)
{
    memcpy(m_allocation.getMappedPtr() + offset, srcData, static_cast<size_t>(size));
    m_device->invalidateMappedRange(m_allocation.getMemory(), m_allocation.offset + offset, size);
}

void StagingVulkanBuffer::updateFromHost(const void* srcData)
{
    memcpy(m_allocation.getMappedPtr(), srcData, m_size);
    m_device->invalidateMappedRange(m_allocation.getMemory(), m_allocation.offset, VK_WHOLE_SIZE);
}

void StagingVulkanBuffer::updateFromStaging(const StagingVulkanBuffer& stagingVulkanBuffer)
{
    memcpy(m_allocation.getMappedPtr(), stagingVulkanBuffer.m_allocation.getMappedPtr(),
        stagingVulkanBuffer.m_allocation.size);
    m_device->invalidateMappedRange(m_allocation.getMemory(), m_allocation.offset, m_allocation.size);
}
} // namespace crisp
