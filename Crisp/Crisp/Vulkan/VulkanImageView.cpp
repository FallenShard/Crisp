#include <Crisp/vulkan/VulkanImageView.hpp>

#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanSampler.hpp>

namespace crisp
{
    VulkanImageView::VulkanImageView(const VulkanDevice& device, const VulkanImage& image, VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel, uint32_t mipLevels)
        : VulkanResource(device.getResourceDeallocator())
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

        vkCreateImageView(device.getHandle(), &viewInfo, nullptr, &m_handle);
    }

    VkDescriptorImageInfo VulkanImageView::getDescriptorInfo(const VulkanSampler* sampler, VkImageLayout layout) const
    {
        return { sampler ? sampler->getHandle() : VK_NULL_HANDLE, m_handle, layout };
    }
}