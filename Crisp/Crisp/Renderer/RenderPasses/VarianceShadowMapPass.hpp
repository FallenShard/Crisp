#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createVarianceShadowMappingPass(Renderer& renderer, unsigned int shadowMapSize);
} // namespace crisp
