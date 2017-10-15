#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class ClearHashGridPipeline : public VulkanPipeline
    {
    public:
        ClearHashGridPipeline(VulkanRenderer* renderer);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_shader;
    };
}
