#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createSsaoPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}