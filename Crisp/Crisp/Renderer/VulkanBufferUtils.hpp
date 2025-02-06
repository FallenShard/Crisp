#pragma once

#include <memory>
#include <span>

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {
std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice& device, VkDeviceSize size, const void* data);

template <typename T>
std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice& device, const std::vector<T>& data) {
    return createStagingBuffer(device, data.size() * sizeof(T), data.data());
}

std::unique_ptr<VulkanBuffer> createVertexBuffer(
    VulkanDevice& device, std::span<const std::byte> data, VkBufferUsageFlags flags = 0);
std::unique_ptr<VulkanBuffer> createVertexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags flags = 0);
std::unique_ptr<VulkanBuffer> createIndexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags flags = 0);
} // namespace crisp