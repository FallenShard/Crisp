#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createDielectricPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass);
}
