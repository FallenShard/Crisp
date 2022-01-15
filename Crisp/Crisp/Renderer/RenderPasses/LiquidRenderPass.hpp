#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createLiquidRenderPass(const VulkanDevice& device, VkExtent2D renderArea);
} // namespace crisp
