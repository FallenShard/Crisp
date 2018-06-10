#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createNormalMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
