#pragma once

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class DefaultRenderPass : public VulkanRenderPass
    {
    public:
        DefaultRenderPass(VulkanRenderer* renderer);
        ~DefaultRenderPass();

        virtual void begin(VkCommandBuffer buffer, VkFramebuffer framebuffer = nullptr) const override;
        virtual VkImage getColorAttachment(unsigned int index) const override;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const override;

    private:
        virtual void createRenderPass() override;
        virtual void createResources() override;
        virtual void freeResources() override;

        VkFormat m_colorFormat;
        VkFormat m_depthFormat;

        std::vector<VkClearValue> m_clearValues;
    };
}