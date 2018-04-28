#pragma once

#include "Vulkan/VulkanPipeline.hpp"

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
        GuiColorQuadPipeline(Renderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}
