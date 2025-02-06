#include <Crisp/Renderer/VulkanBufferUtils.hpp>

namespace crisp {
std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice& device, VkDeviceSize size, const void* data) {
    auto stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, size);
    if (data) {
        stagingBuffer->updateFromHost(data, size, 0);
    }
    return stagingBuffer;
}

std::unique_ptr<VulkanBuffer> createVertexBuffer(
    VulkanDevice& device, const std::span<const std::byte> data, VkBufferUsageFlags extraFlags) {
    static constexpr VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto buffer = std::make_unique<VulkanBuffer>(
        device, data.size(), usageFlags | extraFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, data.size());
    stagingBuffer->updateFromHost(data.data(), data.size(), 0);
    device.getGeneralQueue().submitAndWait([&buffer, &stagingBuffer](const VkCommandBuffer cmdBuffer) {
        buffer->copyFrom(cmdBuffer, *stagingBuffer);
    });
    return buffer;
}

std::unique_ptr<VulkanBuffer> createVertexBuffer(
    VulkanDevice& device, const VkDeviceSize data, VkBufferUsageFlags extraFlags) {
    static constexpr VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return std::make_unique<VulkanBuffer>(device, data, usageFlags | extraFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

std::unique_ptr<VulkanBuffer> createIndexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags extraFlags) {
    static constexpr VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return std::make_unique<VulkanBuffer>(device, size, usageFlags | extraFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
} // namespace crisp