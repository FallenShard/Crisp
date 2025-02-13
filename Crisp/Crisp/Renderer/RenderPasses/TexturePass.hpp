#pragma once

#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

namespace crisp {
std::unique_ptr<VulkanRenderPass> createTexturePass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, VkExtent2D renderArea, VkFormat textureFormat);
std::unique_ptr<VulkanRenderPass> createTexturePass(
    const VulkanDevice& device, VkExtent2D renderArea, VkFormat textureFormat);
} // namespace crisp
