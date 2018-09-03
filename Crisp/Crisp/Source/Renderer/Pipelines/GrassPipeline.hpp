#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createGrassPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
