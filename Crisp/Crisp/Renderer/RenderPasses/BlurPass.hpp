#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp
{
    class BlurPass : public VulkanRenderPass
    {
    public:
        BlurPass(Renderer* renderer, VkFormat format, VkExtent2D renderArea);

    protected:
        virtual void createResources() override;

        VkFormat m_colorFormat;
    };
}
