#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createTexturePass(Renderer& renderer, VkExtent2D renderArea, VkFormat textureFormat,
    bool bufferedRenderTargets);
} // namespace crisp
