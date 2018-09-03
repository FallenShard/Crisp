#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createLightShaftPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}