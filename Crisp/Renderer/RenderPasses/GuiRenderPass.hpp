#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class GuiRenderPass : public VulkanRenderPass
    {
    public:
        GuiRenderPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
