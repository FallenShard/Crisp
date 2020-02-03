#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createPbrPipeline(Renderer* renderer, VulkanRenderPass* renderPass, unsigned int numMaterials);
    std::unique_ptr<VulkanPipeline> createPbrUnifPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
