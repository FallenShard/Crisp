#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class GuiRenderPass : public VulkanRenderPass
    {
    public:
        GuiRenderPass(VulkanRenderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
