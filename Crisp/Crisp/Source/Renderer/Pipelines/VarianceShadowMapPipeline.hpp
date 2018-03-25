#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class VarianceShadowMapPipeline : public VulkanPipeline
    {
    public:
        VarianceShadowMapPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}
