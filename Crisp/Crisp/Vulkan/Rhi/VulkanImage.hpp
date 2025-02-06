#pragma once

#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {

struct VulkanImageDescription {
    VkImageType imageType{VK_IMAGE_TYPE_2D};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_1_BIT};
    VkExtent3D extent{1, 1, 1};
    uint32_t mipLevelCount{1};
    uint32_t layerCount{1};
    VkImageUsageFlags usageFlags{VK_IMAGE_USAGE_SAMPLED_BIT};
    VkImageCreateFlags createFlags{0};
    std::optional<VkImageAspectFlags> aspectMask{std::nullopt};
};

class VulkanImage : public VulkanResource<VkImage> {
public:
    VulkanImage(const VulkanDevice& device, const VulkanImageDescription& imageDescription);
    VulkanImage(const VulkanDevice& device, const VkImageCreateInfo& createInfo);
    VulkanImage(
        const VulkanDevice& device,
        VkExtent3D extent,
        uint32_t numLayers,
        uint32_t numMipmaps,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageCreateFlags createFlags);
    ~VulkanImage();

    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    VulkanImage(VulkanImage&&) noexcept = default;
    VulkanImage& operator=(VulkanImage&&) noexcept = default;

    uint32_t getMipLevels() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    VkImageAspectFlags getAspectMask() const;
    VkFormat getFormat() const;
    uint32_t getLayerCount() const;

    VkImageSubresourceRange getFullRange() const;
    VkImageLayout getLayout() const;
    VkImageLayout getLayout(uint32_t layer, uint32_t mipLevel) const;

    void setImageLayout(VkImageLayout newLayout, VkImageSubresourceRange range);
    void transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, const VulkanSynchronizationScope& scope);
    void transitionLayout(
        VkCommandBuffer buffer,
        VkImageLayout newLayout,
        uint32_t baseLayer,
        uint32_t numLayers,
        const VulkanSynchronizationScope& scope);
    void transitionLayout(
        VkCommandBuffer buffer,
        VkImageLayout newLayout,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t baseLevel,
        uint32_t levelCount,
        const VulkanSynchronizationScope& scope);
    void transitionLayout(
        VkCommandBuffer buffer,
        VkImageLayout newLayout,
        VkImageSubresourceRange subresRange,
        const VulkanSynchronizationScope& scope);

    void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer);
    void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers);
    void copyFrom(
        VkCommandBuffer commandBuffer,
        const VulkanBuffer& buffer,
        const VkExtent3D& extent,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t mipLevel);
    void copyTo(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers);
    void buildMipmaps(VkCommandBuffer commandBuffer, const VulkanSynchronizationStage& stage = kTransferRead);
    void blit(VkCommandBuffer commandBuffer, const VulkanImage& image, uint32_t mipLevel);

    VkImageSubresourceRange getFirstMipRange() const;

    VkExtent2D getExtent2D() const;

private:
    bool matchesLayout(VkImageLayout imageLayout, const VkImageSubresourceRange& range) const;
    bool isSameLayoutInRange(const VkImageSubresourceRange& range) const;

    VkImageLayout getLayoutUnchecked(uint32_t layer, uint32_t mipLevel) const;

    VmaAllocation m_allocation{nullptr};
    VmaAllocationInfo m_allocationInfo{};

    VkImageType m_imageType;

    VkExtent3D m_extent;
    uint32_t m_mipLevelCount;
    uint32_t m_layerCount;

    VkFormat m_format;
    VkSampleCountFlagBits m_sampleCount;

    VkImageAspectFlags m_aspect;

    // Tracks layouts across all layers and mip levels. Stored as layerCount * mipLevelCount.
    std::vector<VkImageLayout> m_layouts;
};

VkImageAspectFlags determineImageAspect(VkFormat format);

bool isDepthFormat(VkFormat format);

const char* toString(VkImageLayout layout);
std::string toString(VkImageUsageFlags flags);

} // namespace crisp