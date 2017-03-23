#pragma once

#include "VulkanPipeline.hpp"

namespace crisp
{
    class LiquidPipeline : public VulkanPipeline
    {
    public:
        LiquidPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}
