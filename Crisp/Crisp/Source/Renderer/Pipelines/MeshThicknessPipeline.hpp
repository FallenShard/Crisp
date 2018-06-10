#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createMeshThicknessPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
