#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/vulkan/VulkanSampler.hpp>

#include <memory>

namespace crisp
{
class VulkanImageView : public VulkanResource<VkImageView>
{
public:
    VulkanImageView(
        const VulkanDevice& device,
        VulkanImage& image,
        VkImageViewType type,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t baseMipLevel = 0,
        uint32_t mipLevels = 1);
    VkDescriptorImageInfo getDescriptorInfo(
        const VulkanSampler* sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;

    VkImageSubresourceRange getSubresourceRange() const
    {
        return m_subresourceRange;
    }

    const VulkanImage& getImage() const
    {
        return m_image;
    }

private:
    VulkanImage& m_image;
    VkImageSubresourceRange m_subresourceRange;
};

std::unique_ptr<VulkanImageView> createView(VulkanImage& image, VkImageViewType type);
std::unique_ptr<VulkanImageView> createView(
    VulkanImage& image,
    VkImageViewType type,
    uint32_t baseLayer,
    uint32_t numLayers,
    uint32_t baseMipLevel = 0,
    uint32_t mipLevels = 1);

} // namespace crisp