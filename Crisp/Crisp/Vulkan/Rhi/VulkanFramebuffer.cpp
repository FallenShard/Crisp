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

std::unique_ptr<VulkanFramebuffer> createFramebuffer(
    const VulkanDevice& device,
    VkRenderPass renderPass,
    const std::vector<std::unique_ptr<VulkanImageView>>& attachments) {
    CRISP_CHECK_NE(attachments.size(), 0);
    const VkExtent2D extent = attachments.front()->getImage().getExtent2D();

    std::vector<VkImageView> attachmentHandles(attachments.size());
    for (const auto& attachment : attachments) {
        CRISP_CHECK_EQ(attachment->getImage().getExtent2D().width, extent.width);
        CRISP_CHECK_EQ(attachment->getImage().getExtent2D().height, extent.height);
        attachmentHandles.push_back(attachment->getHandle());
    }

    return std::make_unique<VulkanFramebuffer>(device, renderPass, extent, attachmentHandles);
}

std::unique_ptr<VulkanFramebuffer> createFramebuffer(
    const VulkanDevice& device, VkRenderPass renderPass, const VulkanImageView& attachment) {
    return std::make_unique<VulkanFramebuffer>(
        device, renderPass, attachment.getImage().getExtent2D(), std::vector<VkImageView>{attachment.getHandle()});
}

} // namespace crisp
