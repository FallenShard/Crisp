#pragma once

#include <vector>
#include <memory>

#include "Vulkan/VulkanRenderPass.hpp"

namespace crisp
{
    class Texture;
    class TextureView;

    class GuiRenderPass : public VulkanRenderPass
    {
    public:
        GuiRenderPass(VulkanRenderer* renderer);
        ~GuiRenderPass();

        virtual void begin(VkCommandBuffer cmdBuffer) const override;
        virtual VkImage getColorAttachment(unsigned int index = 0) const override;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const override;

        std::unique_ptr<TextureView> createRenderTargetView(unsigned int index = 0) const;

        VkFormat getColorFormat() const;

    protected:
        virtual void createRenderPass() override;
        virtual void createResources() override;
        virtual void freeResources() override;

        VkFormat m_colorFormat;
        VkFormat m_depthFormat;

        std::vector<std::shared_ptr<Texture>> m_renderTargets;
        std::vector<std::vector<std::shared_ptr<TextureView>>> m_renderTargetViews;
        std::vector<VkClearValue> m_clearValues;

        std::vector<VkFramebuffer> m_framebuffers;
    };
}
