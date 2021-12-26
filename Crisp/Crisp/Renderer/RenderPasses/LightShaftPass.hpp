#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
class LightShaftPass : public VulkanRenderPass
{
public:
    LightShaftPass(Renderer& renderer);
};
} // namespace crisp
