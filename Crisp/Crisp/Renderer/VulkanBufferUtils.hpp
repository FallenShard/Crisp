#pragma once

#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <memory>

namespace crisp
{
std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice& device, VkDeviceSize size, const void* data);
std::unique_ptr<VulkanBuffer> createVertexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags flags = 0);
std::unique_ptr<VulkanBuffer> createIndexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags flags = 0);
} // namespace crisp