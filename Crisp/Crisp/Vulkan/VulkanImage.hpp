#pragma once

#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

#include <memory>
#include <vector>

namespace crisp
{
class VulkanDevice;
class VulkanBuffer;
class VulkanImageView;

class VulkanImage : public VulkanResource<VkImage, vkDestroyImage>
{
public:
    VulkanImage(VulkanDevice& device, const VkImageCreateInfo& createInfo, VkImageAspectFlags aspect);
    VulkanImage(VulkanDevice& device, VkExtent3D extent, uint32_t numLayers, uint32_t numMipmaps, VkFormat format,
        VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags);
    virtual ~VulkanImage();

    void setImageLayout(VkImageLayout newLayout, uint32_t baseLayer);
    void setImageLayout(VkImageLayout newLayout, VkImageSubresourceRange subresourceRange);
    void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage);
    void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers,
        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
    void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers,
        uint32_t baseLevel, uint32_t levelCount, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
    void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, VkImageSubresourceRange subresRange,
        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

    void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer);
    void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers);
    void buildMipmaps(VkCommandBuffer commandBuffer);
    void blit(VkCommandBuffer commandBuffer, const VulkanImage& image, uint32_t layer);

    std::unique_ptr<VulkanImageView> createView(VkImageViewType type);
    std::unique_ptr<VulkanImageView> createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers,
        uint32_t baseMipLevel = 0, uint32_t mipLevels = 1);

    uint32_t getMipLevels() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    VkImageAspectFlags getAspectMask() const;
    VkFormat getFormat() const;

private:
    std::pair<VkAccessFlags, VkAccessFlags> determineAccessMasks(VkImageLayout oldLayout,
        VkImageLayout newLayout) const;

    VulkanMemoryHeap::Allocation m_allocation;

    VulkanDevice* m_device;

    VkImageType m_type;
    VkFormat m_format;
    VkExtent3D m_extent;
    uint32_t m_mipLevels;
    uint32_t m_numLayers;
    VkImageAspectFlags m_aspect;

    std::vector<std::vector<VkImageLayout>> m_layouts;
};

VkImageAspectFlags determineImageAspect(VkFormat format);

} // namespace crisp