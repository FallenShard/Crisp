#pragma once

#include <memory>
#include <vector>

#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanImage;
    class VulkanImageView;
    class VulkanFramebuffer;
    class VulkanRenderer;

    class VulkanRenderPass : public VulkanResource<VkRenderPass>
    {
    public:
        VulkanRenderPass(VulkanRenderer* renderer);
        virtual ~VulkanRenderPass();
        void recreate();

        VkExtent2D getRenderArea() const;
        VkViewport createViewport() const;
        VkRect2D createScissor() const;

        void begin(VkCommandBuffer cmdBuffer) const;
        void end(VkCommandBuffer cmdBuffer) const;
        void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

        VulkanImage*     getRenderTarget(unsigned int index) const;
        VulkanImageView* getRenderTargetView(unsigned int index, unsigned int frameIndex) const;
        std::unique_ptr<VulkanImageView> createRenderTargetView(unsigned int index, unsigned int numFrames) const;

    protected:
        virtual void createResources() = 0;
        void freeResources();

        VulkanRenderer* m_renderer;

        VkExtent2D m_renderArea;

        std::vector<VkImageLayout> m_finalLayouts;
        std::vector<std::unique_ptr<VulkanImage>> m_renderTargets;
        std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_renderTargetViews;
        std::vector<VkClearValue> m_clearValues;

        std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;
    };
}
