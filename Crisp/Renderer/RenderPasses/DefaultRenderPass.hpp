#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

#include <robin_hood/robin_hood.h>

namespace crisp
{
    class DefaultRenderPass : public VulkanRenderPass
    {
    public:
        DefaultRenderPass(Renderer* renderer);

        void recreateFramebuffer(VkImageView VulkanSwapChainImageView);

    private:
        virtual void createResources() override;

        robin_hood::unordered_flat_map<VkImageView, std::unique_ptr<VulkanFramebuffer>> m_imageFramebuffers;
    };
}