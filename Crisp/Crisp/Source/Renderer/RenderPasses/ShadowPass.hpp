#pragma once

#include <vector>
#include <memory>

#include "Renderer/RenderPasses/VulkanRenderPass.hpp"

namespace crisp
{
    class Texture;
    class TextureView;

    class ShadowPass : public VulkanRenderPass
    {
    public:
        ShadowPass(VulkanRenderer* renderer, unsigned int shadowMapSize, unsigned int numCascades);
        ~ShadowPass();

        virtual void begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer = nullptr) const override;
        virtual VkImage getColorAttachment(unsigned int index = 0) const override;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const override;

        std::unique_ptr<TextureView> createRenderTargetView(unsigned int index = 0) const;

        VkFramebuffer getFramebuffer(unsigned int index) const;

    protected:
        virtual void createRenderPass() override;
        virtual void createResources() override;
        virtual void freeResources() override;

        VkFormat m_depthFormat;

        std::vector<std::shared_ptr<Texture>> m_renderTargets;
        std::vector<std::vector<std::shared_ptr<TextureView>>> m_renderTargetViews;
        std::vector<VkClearValue> m_clearValues;

        std::vector<VkFramebuffer> m_framebuffers;

        unsigned int m_numCascades;
    };
}
