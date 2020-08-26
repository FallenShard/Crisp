#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createColorPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
