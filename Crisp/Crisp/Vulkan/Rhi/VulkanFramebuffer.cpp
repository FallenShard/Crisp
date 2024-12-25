#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>

namespace crisp {
VulkanFramebuffer::VulkanFramebuffer(
    const VulkanDevice& device,
    const VkRenderPass renderPass,
    const VkExtent2D extent,
    const std::vector<VkImageView>& attachmentList,
    const uint32_t layers,
    const VkFramebufferCreateFlags flags)
    : VulkanResource(device.getResourceDeallocator())
    , m_attachments(attachmentList) {
    VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    createInfo.renderPass = renderPass;
    createInfo.width = extent.width;
    createInfo.height = extent.height;
    createInfo.layers = layers;
    createInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
    createInfo.pAttachments = m_attachments.data();
    createInfo.flags = flags;
    vkCreateFramebuffer(device.getHandle(), &createInfo, nullptr, &m_handle);
}
} // namespace crisp
