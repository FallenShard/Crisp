#include "VulkanFramebuffer.hpp"

#include "VulkanDevice.hpp"

namespace crisp
{
    VulkanFramebuffer::VulkanFramebuffer(VulkanDevice* device, VkRenderPass renderPass, VkExtent2D extent, const std::vector<VkImageView>& attachments, uint32_t layers, VkFramebufferCreateFlags flags)
        : VulkanResource(device)
        , m_attachments(attachments)
    {
        VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass      = renderPass;
        createInfo.width           = extent.width;
        createInfo.height          = extent.height;
        createInfo.layers          = layers;
        createInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
        createInfo.pAttachments    = m_attachments.data();
        createInfo.flags           = flags;
        vkCreateFramebuffer(m_device->getHandle(), &createInfo, nullptr, &m_handle);
    }
}


