#pragma once

#include <vector>

#include "VulkanRenderPass.hpp"

namespace crisp
{
    class SceneRenderPass : public VulkanRenderPass
    {
    public:
        enum RenderTarget
        {
            Opaque,
            Depth,
            Composited,

            Count
        };

        SceneRenderPass(VulkanRenderer* renderer);
        ~SceneRenderPass();

        virtual void begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer = nullptr) const override;
        virtual VkImage getColorAttachment(unsigned int index = 0) const override;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const override;

        VkFormat getColorFormat() const;

    protected:
        virtual void createRenderPass() override;
        virtual void createResources() override;
        virtual void freeResources() override;

        VkFormat m_colorFormat;
        VkFormat m_depthFormat;

        std::vector<VkImage> m_renderTargets;
        std::vector<std::vector<VkImageView>> m_renderTargetViews;
        std::vector<VkClearValue> m_clearValues;

        std::vector<VkFramebuffer> m_framebuffers;
    };
}
