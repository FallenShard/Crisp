#pragma once

#include <vector>
#include <memory>

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class Texture;
    class TextureView;
    class VulkanFramebuffer;

    class LiquidRenderPass : public VulkanRenderPass
    {
    public:
        enum RenderTarget
        {
            GBuffer,
            Depth,
            LiquidMask,

            Count
        };

        LiquidRenderPass(VulkanRenderer* renderer);
        ~LiquidRenderPass();

        virtual void begin(VkCommandBuffer cmdBuffer) const override;
        virtual VkImage getColorAttachment(unsigned int index = 0) const override;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const override;

        std::unique_ptr<TextureView> createRenderTargetView(unsigned int index = 0) const;

        VkFormat getColorFormat() const;

        VkFramebuffer getFramebuffer(unsigned int index) const;

    protected:
        virtual void createRenderPass() override;
        virtual void createResources() override;
        virtual void freeResources() override;

        VkFormat m_colorFormat;
        VkFormat m_depthFormat;

        std::vector<std::unique_ptr<Texture>> m_renderTargets;
        std::vector<std::vector<std::unique_ptr<TextureView>>> m_renderTargetViews;
        std::vector<VkClearValue> m_clearValues;

        std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;
    };
}
