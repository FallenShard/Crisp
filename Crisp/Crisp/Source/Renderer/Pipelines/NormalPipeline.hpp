#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createNormalPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
