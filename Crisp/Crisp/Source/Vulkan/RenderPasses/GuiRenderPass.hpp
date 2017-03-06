#pragma once

#include <vector>

#include "VulkanRenderPass.hpp"

namespace crisp
{
    class GuiRenderPass : public VulkanRenderPass
    {
    public:
        GuiRenderPass(VulkanRenderer* renderer);
        ~GuiRenderPass();

        virtual void begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer = nullptr) const override;
        virtual VkImage getColorAttachment(unsigned int index = 0) const override;

        VkFormat getColorFormat() const;

    protected:
        virtual void createRenderPass() override;
        virtual void createResources() override;
        virtual void freeResources() override;

        VkImage m_renderTarget;
        VkImage m_depthTarget;

        VkFormat m_colorFormat;
        VkFormat m_depthFormat;

        std::vector<VkImageView> m_renderTargetViews;
        std::vector<VkImageView> m_depthTargetViews;
        std::vector<VkFramebuffer> m_framebuffers;
        std::vector<VkClearValue> m_clearValues;
    };
}
