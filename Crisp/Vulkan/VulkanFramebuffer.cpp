#include "VulkanFramebuffer.hpp"

#include "VulkanDevice.hpp"

namespace crisp
{
    VulkanFramebuffer::VulkanFramebuffer(VulkanDevice* device, VkRenderPass renderPass, VkExtent2D extent, const std::vector<VkImageView>& attachments)
        : VulkanResource(device)
    {
        VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass      = renderPass;
        createInfo.width           = extent.width;
        createInfo.height          = extent.height;
        createInfo.layers          = 1;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments    = attachments.data();
        vkCreateFramebuffer(m_device->getHandle(), &createInfo, nullptr, &m_handle);
    }

    VulkanFramebuffer::~VulkanFramebuffer()
    {
        m_device->deferDestruction(m_framesToLive, m_handle, [](void* handle, VkDevice device)
        {
            vkDestroyFramebuffer(device, static_cast<VkFramebuffer>(handle), nullptr);
        });
    }
}


