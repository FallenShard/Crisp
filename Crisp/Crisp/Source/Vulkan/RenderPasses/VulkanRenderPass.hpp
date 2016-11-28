#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanRenderer;

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(VulkanRenderer* renderer);
        virtual ~VulkanRenderPass();
        void recreate();

        VkRenderPass getHandle() const;

        virtual void begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer = nullptr) const = 0;
        void end(VkCommandBuffer cmdBuffer) const;

        virtual VkImage getColorAttachment(unsigned int index) const = 0;

    protected:
        virtual void createRenderPass() = 0;
        virtual void createResources() = 0;
        virtual void freeResources() = 0;

        VulkanRenderer* m_renderer;
        VkDevice m_device;
        VkRenderPass m_renderPass;
    };
}
