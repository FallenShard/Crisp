#pragma once

#include <memory>
#include <span>

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {

class Renderer;

std::unique_ptr<VulkanBuffer> createVertexBuffer(
    Renderer& renderer, std::span<const std::byte> data, VkBufferUsageFlags flags = 0);
std::unique_ptr<VulkanBuffer> createVertexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags flags = 0);
std::unique_ptr<VulkanBuffer> createIndexBuffer(VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags flags = 0);
} // namespace crisp