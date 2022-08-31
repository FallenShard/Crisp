#pragma once

#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createShadowMapPass(
    const VulkanDevice& device,
    RenderTargetCache& renderTargetCache,
    uint32_t shadowMapSize,
    uint32_t cascadeIndex = 0);

std::unique_ptr<VulkanRenderPass> createVarianceShadowMappingPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, uint32_t shadowMapSize);

} // namespace crisp
