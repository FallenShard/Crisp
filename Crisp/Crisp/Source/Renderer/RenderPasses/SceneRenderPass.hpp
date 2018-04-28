#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class SceneRenderPass : public VulkanRenderPass
    {
    public:
        enum RenderTarget
        {
            Opaque,
            Depth,

            Count
        };

        SceneRenderPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
