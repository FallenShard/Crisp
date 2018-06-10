#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createSkyboxPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass);
}
