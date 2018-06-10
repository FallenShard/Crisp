#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createBlurPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}