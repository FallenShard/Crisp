#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class DefaultRenderPass : public VulkanRenderPass
    {
    public:
        DefaultRenderPass(VulkanRenderer* renderer);

        void recreateFramebuffer(VkImageView swapChainImageView);

    private:
        virtual void createResources() override;
    };
}