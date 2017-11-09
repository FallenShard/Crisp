#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class DefaultRenderPass : public VulkanRenderPass
    {
    public:
        DefaultRenderPass(VulkanRenderer* renderer);
        ~DefaultRenderPass();

        virtual void begin(VkCommandBuffer buffer) const override;
        virtual VkImage getColorAttachment(unsigned int index) const override;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const override;

        void recreateFramebuffer(VkImageView swapChainImageView);

    private:
        virtual void createRenderPass() override;
        virtual void createResources() override;
        virtual void freeResources() override;

        VkFormat m_colorFormat;
        VkClearValue m_clearValue;

        std::vector<VkFramebuffer> m_framebuffers;
    };
}