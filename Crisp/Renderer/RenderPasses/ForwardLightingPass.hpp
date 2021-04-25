#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class ForwardLightingPass : public VulkanRenderPass
    {
    public:
        ForwardLightingPass(Renderer* renderer, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

    protected:
        virtual void createResources() override;
    };
}
