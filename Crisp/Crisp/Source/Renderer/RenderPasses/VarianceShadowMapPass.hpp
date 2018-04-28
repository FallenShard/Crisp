#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class VarianceShadowMapPass : public VulkanRenderPass
    {
    public:
        VarianceShadowMapPass(Renderer* renderer, unsigned int shadowMapSize);

    protected:
        virtual void createResources() override;
    };
}
