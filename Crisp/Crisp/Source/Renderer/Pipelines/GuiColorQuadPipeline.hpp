#pragma once

#include "VulkanPipeline.hpp"

namespace crisp
{
    class GuiColorQuadPipeline : public VulkanPipeline
    {
    public:
        enum DescSets
        {
            TransformAndColor,

            Count
        };
        GuiColorQuadPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}