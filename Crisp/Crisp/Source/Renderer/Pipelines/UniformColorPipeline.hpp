#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createColorPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
    std::unique_ptr<VulkanPipeline> createDiffusePipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
