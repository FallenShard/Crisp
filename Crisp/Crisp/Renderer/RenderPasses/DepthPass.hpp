#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createDepthPass(Renderer& renderer);
} // namespace crisp
