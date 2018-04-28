#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class GuiTexQuadPipeline : public VulkanPipeline
    {
    public:
        GuiTexQuadPipeline(Renderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}
