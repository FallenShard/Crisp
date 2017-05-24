#pragma once

#include "VulkanPipeline.hpp"

namespace crisp
{
    class ShadowMapPipeline : public VulkanPipeline
    {
    public:
        ShadowMapPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass = 0);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;

        uint32_t m_subpass;
    };
}
