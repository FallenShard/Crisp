#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class SceneRenderPass : public VulkanRenderPass
    {
    public:
        SceneRenderPass(Renderer* renderer, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

    protected:
        virtual void createResources() override;
    };
}
