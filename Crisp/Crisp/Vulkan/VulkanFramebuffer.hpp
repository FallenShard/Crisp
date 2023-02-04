#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
class VulkanDevice;

class VulkanFramebuffer : public VulkanResource<VkFramebuffer>
{
public:
    VulkanFramebuffer(
        const VulkanDevice& device,
        VkRenderPass renderPass,
        VkExtent2D extent,
        const std::vector<VkImageView>& attachmentList,
        uint32_t layers = 1,
        VkFramebufferCreateFlags flags = 0);

    VkImageView getAttachment(uint32_t idx) const
    {
        return m_attachments.at(idx);
    }

private:
    std::vector<VkImageView> m_attachments;
};
} // namespace crisp
