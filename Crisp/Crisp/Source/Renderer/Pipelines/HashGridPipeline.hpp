#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class HashGridPipeline : public VulkanPipeline
    {
    public:
        HashGridPipeline(VulkanRenderer* renderer);

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_shader;
    };
}
