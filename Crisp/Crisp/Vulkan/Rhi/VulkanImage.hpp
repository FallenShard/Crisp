#pragma once

#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanMemoryHeap.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {
class VulkanImage : public VulkanResource<VkImage> {
public:
    VulkanImage(const VulkanDevice& device, const VkImageCreateInfo& createInfo);
    VulkanImage(
        const VulkanDevice& device,
        VkExtent3D extent,
        uint32_t numLayers,
        uint32_t numMipmaps,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageCreateFlags createFlags);
    ~VulkanImage() override;

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

    void setImageLayout(VkImageLayout newLayout, VkImageSubresourceRange range);
    void transitionLayout(
        VkCommandBuffer buffer, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
    void transitionLayout(
        VkCommandBuffer buffer,
        VkImageLayout newLayout,
        uint32_t baseLayer,
        uint32_t numLayers,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage);
    void transitionLayout(
        VkCommandBuffer buffer,
        VkImageLayout newLayout,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t baseLevel,
        uint32_t levelCount,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage);
    void transitionLayout(
        VkCommandBuffer buffer,
        VkImageLayout newLayout,
        VkImageSubresourceRange subresRange,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage);
    void transitionLayoutDirect(
        VkCommandBuffer cmdBuffer,
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStage,
        VkAccessFlags srcAccessMask,
        VkPipelineStageFlags dstStage,
        VkAccessFlags dstAccessMask);

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
    void buildMipmaps(VkCommandBuffer commandBuffer);
    void blit(VkCommandBuffer commandBuffer, const VulkanImage& image, uint32_t mipLevel);

private:
    bool matchesLayout(VkImageLayout imageLayout, const VkImageSubresourceRange& range) const;
    bool isSameLayoutInRange(const VkImageSubresourceRange& range) const;
    std::pair<VkAccessFlags, VkAccessFlags> determineAccessMasks(VkImageLayout oldLayout, VkImageLayout newLayout) const;

    VulkanMemoryHeap::Allocation m_allocation;

    VkImageType m_imageType;

    VkExtent3D m_extent;
    uint32_t m_mipLevelCount;
    uint32_t m_layerCount;

    VkFormat m_format;
    VkSampleCountFlagBits m_sampleCount;

    VkImageAspectFlags m_aspect;

    // Tracks layouts across all layers and mip levels. Indexed as [layer][mipLevel].
    std::vector<std::vector<VkImageLayout>> m_layouts;
};

VkImageAspectFlags determineImageAspect(VkFormat format);

bool isDepthFormat(VkFormat format);

const char* toString(VkImageLayout layout);
std::string toString(VkImageUsageFlags flags);

} // namespace crisp