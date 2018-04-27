#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class AmbientOcclusionPass : public VulkanRenderPass
    {
    public:
        AmbientOcclusionPass(VulkanRenderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
