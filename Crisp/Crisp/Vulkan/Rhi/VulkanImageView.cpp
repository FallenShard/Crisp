#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
VulkanImageView::VulkanImageView(
    const VulkanDevice& device,
    VulkanImage& image,
    VkImageViewType type,
    uint32_t baseLayer,
    uint32_t numLayers,
    uint32_t baseMipLevel,
    uint32_t mipLevels)
    : VulkanResource(device.getResourceDeallocator())
    , m_image(image)
    , m_createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO} {
    m_createInfo.image = image.getHandle();
    m_createInfo.viewType = type;
    m_createInfo.format = image.getFormat();
    m_createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_createInfo.subresourceRange.aspectMask = image.getAspectMask();
    m_createInfo.subresourceRange.baseMipLevel = baseMipLevel;
    m_createInfo.subresourceRange.levelCount = mipLevels;
    m_createInfo.subresourceRange.baseArrayLayer = baseLayer;
    m_createInfo.subresourceRange.layerCount = numLayers;

    VK_FATAL(vkCreateImageView(device.getHandle(), &m_createInfo, nullptr, &m_handle));
}

VkDescriptorImageInfo VulkanImageView::getDescriptorInfo(const VulkanSampler* sampler, VkImageLayout layout) const {
    return {sampler ? sampler->getHandle() : VK_NULL_HANDLE, m_handle, layout};
}

VkImageViewType getImageViewType(const VkImageType imageType, const uint32_t layerCount, const bool isCubemap) {
    if (isCubemap) {
        CRISP_CHECK_EQ(layerCount, 6);
        return VK_IMAGE_VIEW_TYPE_CUBE;
    }

    switch (imageType) {
    case VK_IMAGE_TYPE_1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case VK_IMAGE_TYPE_2D:
        return layerCount == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case VK_IMAGE_TYPE_3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    default:
        CRISP_FATAL("Unknown image type!");
    }
}

std::unique_ptr<VulkanImageView> createView(const VulkanDevice& device, VulkanImage& image, VkImageViewType type) {
    return std::make_unique<VulkanImageView>(device, image, type, 0, image.getLayerCount(), 0, image.getMipLevels());
}

std::unique_ptr<VulkanImageView> createView(
    const VulkanDevice& device,
    VulkanImage& image,
    VkImageViewType type,
    uint32_t baseLayer,
    uint32_t numLayers,
    uint32_t baseMipLevel,
    uint32_t mipLevels) {
    if (type == VK_IMAGE_VIEW_TYPE_CUBE) {
        numLayers = 6;
    }
    return std::make_unique<VulkanImageView>(device, image, type, baseLayer, numLayers, baseMipLevel, mipLevels);
}

} // namespace crisp
