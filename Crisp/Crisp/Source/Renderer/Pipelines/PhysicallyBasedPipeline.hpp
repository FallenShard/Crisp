#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createPbrPipeline(Renderer* renderer, VulkanRenderPass* renderPass, unsigned int numMaterials);
    std::unique_ptr<VulkanPipeline> createPbrUnifPipeline(Renderer* renderer, VulkanRenderPass* renderPass);

    std::unique_ptr<VulkanPipeline> createIrradiancePipeline(Renderer* renderer, VulkanRenderPass* renderPass);
    std::unique_ptr<VulkanPipeline> createEnvMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
