#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanSwapChain;
    class VulkanContext;
    class VulkanDevice;

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(const VulkanDevice& device, VkFormat colorFormat, VkFormat depthStencilFormat);
        ~VulkanRenderPass();
        void recreate();

        VkRenderPass getHandle() const;

    private:
        void create();
        void freeResources();

        VkDevice m_device;
        VkRenderPass m_renderPass;

        VkFormat m_colorFormat;
        VkFormat m_depthStencilFormat;
    };
}
