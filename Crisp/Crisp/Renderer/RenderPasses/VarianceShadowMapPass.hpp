#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createVarianceShadowMappingPass(const VulkanDevice& device,
    unsigned int shadowMapSize);
} // namespace crisp
