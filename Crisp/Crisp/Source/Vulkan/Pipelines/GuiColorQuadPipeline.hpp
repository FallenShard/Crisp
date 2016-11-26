#pragma once

#include "VulkanPipeline.hpp"

namespace crisp
{
    class GuiColorQuadPipeline : public VulkanPipeline
    {
    public:
        GuiColorQuadPipeline(VulkanRenderer* renderer);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_vertShader;
        VkShaderModule m_fragShader;
    };
}
