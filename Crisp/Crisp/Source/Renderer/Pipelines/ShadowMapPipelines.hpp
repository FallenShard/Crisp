#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass);
    std::unique_ptr<VulkanPipeline> createVarianceShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
