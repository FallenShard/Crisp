#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class ComputePressurePipeline : public VulkanPipeline
    {
    public:
        ComputePressurePipeline(VulkanRenderer* renderer);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_shader;
    };
}
