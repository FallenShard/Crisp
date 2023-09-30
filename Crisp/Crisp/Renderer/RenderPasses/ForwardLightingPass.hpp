#pragma once

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp {
std::unique_ptr<VulkanRenderPass> createForwardLightingPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, VkExtent2D renderArea);
} // namespace crisp
