#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createTonemappingPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass, bool useGammaCorrection);
}