#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createLightShaftPass(const VulkanDevice& device, VkExtent2D renderArea);
} // namespace crisp
