#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class EnvironmentRenderPass : public VulkanRenderPass
    {
    public:
        EnvironmentRenderPass(Renderer* renderer, VkExtent2D renderArea);

    protected:
        virtual void createResources() override;
    };
}
