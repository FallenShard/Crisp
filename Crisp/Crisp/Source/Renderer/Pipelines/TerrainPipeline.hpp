#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class TerrainPipeline : public VulkanPipeline
    {
    public:
        TerrainPipeline(Renderer* renderer, VulkanRenderPass* renderPass);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_tescShader;
        VkShaderModule m_teseShader;
        VkShaderModule m_fragShader;
    };
}
