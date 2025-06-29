#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("VulkanBuffer");
} // namespace

VulkanBuffer::VulkanBuffer(
    const VulkanDevice& device,
    const VkDeviceSize size,
    const VkBufferUsageFlags usageFlags,
    const VkMemoryPropertyFlags memProps)
    : VulkanResource(device.getResourceDeallocator())
    , m_allocation(nullptr)
    , m_allocationInfo{}
    , m_size(size)
    , m_address{} {
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocInfo{.usage = VMA_MEMORY_USAGE_AUTO};
    if (memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VK_CHECK(vmaCreateBuffer(
        device.getMemoryAllocator(), &bufferInfo, &allocInfo, &m_handle, &m_allocation, &m_allocationInfo));

    if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo getAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        getAddressInfo.buffer = m_handle;
        m_address = vkGetBufferDeviceAddress(device.getHandle(), &getAddressInfo);
    }
}

VulkanBuffer::~VulkanBuffer() {
    if (m_allocation) {
        m_deallocator->deferMemoryDeallocation(kRendererVirtualFrameCount, m_allocation);
    }
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : VulkanResource(std::move(other))
    , m_allocation(std::exchange(other.m_allocation, nullptr))
    , m_allocationInfo(other.m_allocationInfo)
    , m_size(other.m_size)
    , m_address(other.m_address) {}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    VulkanResource::operator=(std::move(other));
    m_allocation = std::exchange(other.m_allocation, nullptr);
    m_allocationInfo = other.m_allocationInfo;
    m_size = other.m_size;
    m_address = other.m_address;
    return *this;
}

VkDeviceSize VulkanBuffer::getSize() const {
    return m_size;
}

VkDeviceAddress VulkanBuffer::getDeviceAddress() const {
    return m_address;
}

void VulkanBuffer::copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer) const {
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = m_size;
    vkCmdCopyBuffer(cmdBuffer, srcBuffer.m_handle, m_handle, 1, &copyRegion);
}

void VulkanBuffer::copyFrom(
    const VkCommandBuffer cmdBuffer,
    const VulkanBuffer& srcBuffer,
    const VkDeviceSize srcOffset,
    const VkDeviceSize dstOffset,
    const VkDeviceSize size) const {
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmdBuffer, srcBuffer.m_handle, m_handle, 1, &copyRegion);
}

VkDescriptorBufferInfo VulkanBuffer::createDescriptorInfo(VkDeviceSize offset, VkDeviceSize size) const {
    return {m_handle, offset, size};
}

VkDescriptorBufferInfo VulkanBuffer::createDescriptorInfo() const {
    return {m_handle, 0, m_size};
}

StagingVulkanBuffer::StagingVulkanBuffer(
    VulkanDevice& device,
    const VkDeviceSize size,
    const VkBufferUsageFlags usageFlags,
    const VkMemoryPropertyFlags memProps)
    : VulkanBuffer(
          device, size, usageFlags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memProps | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    , m_device(&device) {}

void StagingVulkanBuffer::updateFromHost(
    const void* hostMemoryData, const VkDeviceSize size, const VkDeviceSize offset) {
    memcpy(reinterpret_cast<char*>(m_allocationInfo.pMappedData) + offset, hostMemoryData, size); // NOLINT
    vmaInvalidateAllocation(m_device->getMemoryAllocator(), m_allocation, offset, size);
}

void StagingVulkanBuffer::updateFromHost(const void* hostMemoryData) {
    memcpy(reinterpret_cast<char*>(m_allocationInfo.pMappedData), hostMemoryData, m_size); // NOLINT
    vmaInvalidateAllocation(m_device->getMemoryAllocator(), m_allocation, 0, VK_WHOLE_SIZE);
}

void StagingVulkanBuffer::updateFromStaging(const StagingVulkanBuffer& stagingVulkanBuffer) {
    memcpy(
        reinterpret_cast<char*>(m_allocationInfo.pMappedData),                     // NOLINT
        reinterpret_cast<char*>(stagingVulkanBuffer.m_allocationInfo.pMappedData), // NOLINT
        stagingVulkanBuffer.m_allocationInfo.size);
    vmaInvalidateAllocation(m_device->getMemoryAllocator(), m_allocation, 0, VK_WHOLE_SIZE);
}
} // namespace crisp
