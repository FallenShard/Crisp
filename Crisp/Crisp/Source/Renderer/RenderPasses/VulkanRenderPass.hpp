#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;
    class VulkanRenderer;

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(VulkanRenderer* renderer);
        virtual ~VulkanRenderPass();
        void recreate();

        VkRenderPass getHandle() const;
        VkExtent2D getRenderArea() const;

        virtual void begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer = nullptr) const = 0;
        void end(VkCommandBuffer cmdBuffer) const;

        virtual VkImage getColorAttachment(unsigned int index) const = 0;
        virtual VkImageView getAttachmentView(unsigned int index, unsigned int frameIndex) const = 0;

    protected:
        virtual void createRenderPass() = 0;
        virtual void createResources() = 0;
        virtual void freeResources() = 0;

        VulkanRenderer* m_renderer;
        VulkanDevice*   m_device;
        VkRenderPass    m_renderPass;

        VkExtent2D m_renderArea;
    };
}
