#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class ComputeTestPipeline : public VulkanPipeline
    {
    public:
        ComputeTestPipeline(Renderer* renderer);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_shader;
    };
}
