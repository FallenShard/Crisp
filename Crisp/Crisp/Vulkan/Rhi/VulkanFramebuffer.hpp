#pragma once

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {
class VulkanFramebuffer : public VulkanResource<VkFramebuffer> {
public:
    VulkanFramebuffer(
        const VulkanDevice& device,
        VkRenderPass renderPass,
        VkExtent2D extent,
        const std::vector<VkImageView>& attachmentList,
        uint32_t layers = 1,
        VkFramebufferCreateFlags flags = 0);

    VkImageView getAttachment(uint32_t idx) const {
        return m_attachments.at(idx);
    }

private:
    std::vector<VkImageView> m_attachments;
};

std::unique_ptr<VulkanFramebuffer> createFramebuffer(
    const VulkanDevice& device,
    VkRenderPass renderPass,
    const std::vector<std::unique_ptr<VulkanImageView>>& attachments);

std::unique_ptr<VulkanFramebuffer> createFramebuffer(
    const VulkanDevice& device, VkRenderPass renderPass, const VulkanImageView& attachment);

} // namespace crisp
