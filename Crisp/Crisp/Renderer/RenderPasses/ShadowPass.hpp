#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createShadowMapPass(
    const VulkanDevice& device, uint32_t shadowMapSize, uint32_t cascadeCount);

} // namespace crisp
