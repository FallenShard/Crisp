#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
    class DepthPass : public VulkanRenderPass
    {
    public:
        enum RenderTarget
        {
            Depth,

            Count
        };

        DepthPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
