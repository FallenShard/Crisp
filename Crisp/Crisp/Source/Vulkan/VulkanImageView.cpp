#include "VulkanImageView.hpp"

#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanSampler.hpp"

namespace crisp
{
    VulkanImageView::VulkanImageView(VulkanDevice* device, const VulkanImage& image, VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel, uint32_t mipLevels)
        : VulkanResource(device)
        , m_image(image)
    {
        VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image        = image.getHandle();
        viewInfo.viewType     = type;
        viewInfo.format       = image.getFormat();
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = image.getAspectMask();
        viewInfo.subresourceRange.baseMipLevel   = baseMipLevel;
        viewInfo.subresourceRange.levelCount     = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = baseLayer;
        viewInfo.subresourceRange.layerCount     = numLayers;

        m_subresourceRange = viewInfo.subresourceRange;

        vkCreateImageView(m_device->getHandle(), &viewInfo, nullptr, &m_handle);
    }

    VulkanImageView::~VulkanImageView()
    {
        vkDestroyImageView(m_device->getHandle(), m_handle, nullptr);
    }

    VkDescriptorImageInfo VulkanImageView::getDescriptorInfo(VkSampler sampler, VkImageLayout layout) const
    {
        return { sampler, m_handle, layout };
    }
    VkDescriptorImageInfo VulkanImageView::getDescriptorInfo(const VulkanSampler* sampler, VkImageLayout layout) const
    {
        return { sampler->getHandle(), m_handle, layout };
    }
}