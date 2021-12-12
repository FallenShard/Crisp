#pragma once

#include <memory>

#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
    class VulkanDevice;
    class VulkanImage;
    class VulkanSampler;

    class VulkanImageView : public VulkanResource<VkImageView, vkDestroyImageView>
    {
    public:
        VulkanImageView(VulkanDevice& device, const VulkanImage& image, VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel = 0, uint32_t mipLevels = 1);

        VkDescriptorImageInfo getDescriptorInfo(const VulkanSampler* sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;

        VkImageSubresourceRange getSubresourceRange() const { return m_subresourceRange; }

        const VulkanImage& getImage() const { return m_image; }

    private:
        const VulkanImage& m_image;
        VkImageSubresourceRange m_subresourceRange;
    };
}