#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanRenderer;

    class GuiRenderPass
    {
    public:
        GuiRenderPass(VulkanRenderer* renderer);
        ~GuiRenderPass();
        void recreate();

        VkRenderPass getHandle() const;

    private:
        void create();
        void freeResources();

        VulkanRenderer* m_renderer;
        VkDevice m_device;

        VkRenderPass m_renderPass;
        VkFormat m_colorFormat;
        VkFormat m_depthStencilFormat;

        VkImage m_renderTarget;
        VkImage m_depthTarget;

        std::vector<VkImageView> m_renderTargetViews;
        std::vector<VkImageView> m_depthTargetViews;
        std::vector<VkFramebuffer> m_framebuffers;
    };
}
