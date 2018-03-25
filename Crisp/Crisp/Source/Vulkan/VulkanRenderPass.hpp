#pragma once

#include <vector>

#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanDevice;
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

        virtual void begin(VkCommandBuffer cmdBuffer) const = 0;
        void end(VkCommandBuffer cmdBuffer) const;
        void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

        virtual VkImage getColorAttachment(unsigned int index) const = 0;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const = 0;

    protected:
        virtual void createRenderPass() = 0;
        virtual void createResources() = 0;
        virtual void freeResources() = 0;

        VulkanRenderer* m_renderer;

        VkExtent2D m_renderArea;
    };
}
