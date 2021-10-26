#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
    class AmbientOcclusionPass : public VulkanRenderPass
    {
    public:
        AmbientOcclusionPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
