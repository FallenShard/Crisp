#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class BlurPass : public VulkanRenderPass
    {
    public:
        BlurPass(VulkanRenderer* renderer, VkFormat format, VkExtent2D renderArea);

    protected:
        virtual void createResources() override;

        VkFormat m_colorFormat;
    };
}
