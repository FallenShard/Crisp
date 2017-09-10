#pragma once

#include "VulkanPipeline.hpp"

namespace crisp
{
    class FullScreenQuadPipeline : public VulkanPipeline
    {
    public:
        enum DescSets
        {
            DisplayedImage,

            Count
        };
        FullScreenQuadPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass, bool useGammaCorrection = false);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}