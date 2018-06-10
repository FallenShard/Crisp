#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createBlinnPhongShadowPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
    std::unique_ptr<VulkanPipeline> createBlinnPhongPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
