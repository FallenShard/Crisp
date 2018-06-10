#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createTerrainPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
