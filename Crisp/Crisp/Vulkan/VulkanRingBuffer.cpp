#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
namespace {

CRISP_MAKE_LOGGER_ST("VulkanRingBuffer");

} // namespace

VulkanRingBuffer::VulkanRingBuffer(
    const gsl::not_null<VulkanDevice*> device,
    const VkBufferUsageFlags2 usageFlags,
    const VkDeviceSize size,
    const void* data)
    : m_size(size) {
    m_buffer = std::make_unique<VulkanBuffer>(
        *device, size, usageFlags | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_stagingBuffer = std::make_unique<StagingVulkanBuffer>(*device, size * kRendererVirtualFrameCount);

    if (data) {
        updateStagingBuffer({.data = data, .size = size}, 0);
        device->getGeneralQueue().submitAndWait([this](const VkCommandBuffer cmdBuffer) {
            updateDeviceBuffer(cmdBuffer);
        });
    }
}

void VulkanRingBuffer::updateStagingBuffer(const MemoryCopyRegion& region, const uint32_t regionIndex) {
    const VkDeviceSize offset = regionIndex * m_size + region.dstOffset;
    CRISP_CHECK_LE(offset + region.size, m_stagingBuffer->getSize());
    m_stagingBuffer->updateFromHost(region.data, region.size, offset);

    m_hasUpdate = true;
    m_lastUpdatedRegion = regionIndex;
}

void VulkanRingBuffer::updateDeviceBuffer(const VkCommandBuffer commandBuffer) {
    if (!m_hasUpdate) {
        return;
    }

    m_buffer->copyFrom(commandBuffer, *m_stagingBuffer, m_lastUpdatedRegion * m_size, 0, m_size);

    m_hasUpdate = false;
}

VkDescriptorBufferInfo VulkanRingBuffer::getDescriptorInfo() const {
    return {m_buffer->getHandle(), 0, m_size};
}

VkDescriptorBufferInfo VulkanRingBuffer::getDescriptorInfo(const VkDeviceSize offset, const VkDeviceSize range) const {
    return {m_buffer->getHandle(), offset, range};
}

std::unique_ptr<VulkanRingBuffer> createStorageRingBuffer(
    const gsl::not_null<VulkanDevice*> device, const VkDeviceSize size, const void* data) {
    return std::make_unique<VulkanRingBuffer>(device, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, size, data);
}

std::unique_ptr<VulkanRingBuffer> createUniformRingBuffer(
    const gsl::not_null<VulkanDevice*> device, const VkDeviceSize size, const void* data) {
    return std::make_unique<VulkanRingBuffer>(device, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, size, data);
}

} // namespace crisp