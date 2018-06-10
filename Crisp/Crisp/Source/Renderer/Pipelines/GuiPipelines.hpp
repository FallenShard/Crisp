#pragma once

#include "vulkan/VulkanPipeline.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createGuiColorPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
    std::unique_ptr<VulkanPipeline> createGuiDebugPipeline(Renderer* renderer, VulkanRenderPass* renderPass);
    std::unique_ptr<VulkanPipeline> createGuiTextPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t maxFontTextures);
    std::unique_ptr<VulkanPipeline> createGuiTexturePipeline(Renderer* renderer, VulkanRenderPass* renderPass);
}