#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createOutlinePipeline(Renderer* renderer, const VulkanRenderPass* renderPass);
}
