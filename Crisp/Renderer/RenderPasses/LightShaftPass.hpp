#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class LightShaftPass : public VulkanRenderPass
    {
    public:
        LightShaftPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
