#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class LiquidRenderPass : public VulkanRenderPass
    {
    public:
        enum RenderTarget
        {
            GBuffer,
            Depth,
            LiquidMask,

            Count
        };

        LiquidRenderPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
