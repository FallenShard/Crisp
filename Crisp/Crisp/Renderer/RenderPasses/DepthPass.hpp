#pragma once

#include <Crisp/Renderer/VulkanRenderPassBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

namespace crisp {
inline constexpr uint32_t DepthPassDepthAttachmentIndex{0};

std::unique_ptr<VulkanRenderPass> createDepthPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, VkExtent2D renderArea);
} // namespace crisp
