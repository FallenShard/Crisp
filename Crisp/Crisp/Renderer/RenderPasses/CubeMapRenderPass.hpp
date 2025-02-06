#pragma once

#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

namespace crisp {

std::unique_ptr<VulkanRenderPass> createCubeMapPass(
    const VulkanDevice& device, VkExtent2D renderArea, VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT);

} // namespace crisp
