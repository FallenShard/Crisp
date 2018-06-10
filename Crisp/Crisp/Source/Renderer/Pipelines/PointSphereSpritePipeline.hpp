#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createPointSphereSpritePipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
