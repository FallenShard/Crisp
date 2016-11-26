#pragma once

#include "VulkanPipeline.hpp"

namespace crisp
{
    class FullScreenQuadPipeline : public VulkanPipeline
    {
    public:
        FullScreenQuadPipeline(VulkanRenderer* renderer);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}