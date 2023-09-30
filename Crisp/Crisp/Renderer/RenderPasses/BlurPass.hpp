#pragma once

#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp {
std::unique_ptr<VulkanRenderPass> createBlurPass(
    const VulkanDevice& device,
    RenderTargetCache& renderTargetCache,
    VkFormat format,
    VkExtent2D renderArea,
    bool isSwapChainDependent,
    std::string&& renderTargetName = "BlurMap");
} // namespace crisp
