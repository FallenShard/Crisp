#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class DefaultRenderPass : public VulkanRenderPass
    {
    public:
        DefaultRenderPass(Renderer* renderer);

        void recreateFramebuffer(VkImageView VulkanSwapChainImageView);

    private:
        virtual void createResources() override;
    };
}