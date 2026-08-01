#include <Crisp/Renderer/VulkanBufferUtils.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

namespace crisp {

std::unique_ptr<VulkanBuffer> createVertexBuffer(
    Renderer& renderer, const std::span<const std::byte> data, VkBufferUsageFlags extraFlags) {
    static constexpr VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto& device = renderer.getDevice();
    auto buffer =
        std::make_unique<VulkanBuffer>(device, data.size(), usageFlags | extraFlags, BufferMemoryType::GpuOnly);
    uploadBufferBlocking(device, device.getGeneralQueue(), *buffer, data.data(), data.size());
    return buffer;
}

std::unique_ptr<VulkanBuffer> createVertexBuffer(
    VulkanDevice& device, const VkDeviceSize data, VkBufferUsageFlags extraFlags) {
    static constexpr VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return std::make_unique<VulkanBuffer>(device, data, usageFlags | extraFlags, BufferMemoryType::GpuOnly);
}

std::unique_ptr<VulkanBuffer> createIndexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags extraFlags) {
    static constexpr VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return std::make_unique<VulkanBuffer>(device, size, usageFlags | extraFlags, BufferMemoryType::GpuOnly);
}
} // namespace crisp