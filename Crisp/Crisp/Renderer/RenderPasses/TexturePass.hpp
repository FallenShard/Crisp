#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createTexturePass(const VulkanDevice& device, VkExtent2D renderArea,
    VkFormat textureFormat, bool bufferedRenderTargets);
} // namespace crisp
