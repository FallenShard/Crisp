#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createCubeMapPass(
    const VulkanDevice& device, RenderTarget* renderTarget, VkExtent2D renderArea, uint32_t mipLevelIndex = 0);

} // namespace crisp
