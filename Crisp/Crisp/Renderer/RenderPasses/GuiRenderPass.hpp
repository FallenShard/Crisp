#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

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
