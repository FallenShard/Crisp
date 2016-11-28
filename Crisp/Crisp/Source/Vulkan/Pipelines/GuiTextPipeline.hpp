#pragma once

#include "VulkanPipeline.hpp"

namespace crisp
{
    class GuiTextPipeline : public VulkanPipeline
    {
    public:
        GuiTextPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}
