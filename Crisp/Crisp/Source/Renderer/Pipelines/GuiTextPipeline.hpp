#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class GuiTextPipeline : public VulkanPipeline
    {
    public:
        enum DescSets
        {
            TransformAndColor,
            FontAtlas,

            Count
        };

        GuiTextPipeline(Renderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}
