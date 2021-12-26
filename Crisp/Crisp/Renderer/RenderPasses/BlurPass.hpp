#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createBlurPass(Renderer& renderer, VkFormat format,
    std::optional<VkExtent2D> renderArea = {});
} // namespace crisp
