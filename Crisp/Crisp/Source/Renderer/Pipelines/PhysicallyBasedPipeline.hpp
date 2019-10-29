#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createPbrPipeline(Renderer* renderer, VulkanRenderPass* renderPass, unsigned int numMaterials);
    std::unique_ptr<VulkanPipeline> createPbrUnifPipeline(Renderer* renderer, VulkanRenderPass* renderPass);

    std::unique_ptr<VulkanPipeline> createEquirectToCubeMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass, int subpass);
    std::unique_ptr<VulkanPipeline> createConvolvePipeline(Renderer* renderer, VulkanRenderPass* renderPass, int subpass);
    std::unique_ptr<VulkanPipeline> createPrefilterPipeline(Renderer* renderer, VulkanRenderPass* renderPass, int subpass);
    std::unique_ptr<VulkanPipeline> createBrdfLutPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}
