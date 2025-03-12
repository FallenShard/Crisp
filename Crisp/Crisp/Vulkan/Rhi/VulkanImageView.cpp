#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

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
    , m_subresourceRange{} {
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image.getHandle();
    viewInfo.viewType = type;
    viewInfo.format = image.getFormat();
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = image.getAspectMask();
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = baseLayer;
    viewInfo.subresourceRange.layerCount = numLayers;

    m_subresourceRange = viewInfo.subresourceRange;

    vkCreateImageView(device.getHandle(), &viewInfo, nullptr, &m_handle);
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