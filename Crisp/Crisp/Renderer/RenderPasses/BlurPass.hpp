#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createBlurPass(const VulkanDevice& device, VkFormat format, VkExtent2D renderArea,
    bool isSwapChainDependent);
} // namespace crisp
