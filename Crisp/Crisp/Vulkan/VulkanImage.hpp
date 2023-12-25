#pragma once

#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

#include <vector>

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

    void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer);
    void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers);
    void copyFrom(
        VkCommandBuffer commandBuffer,
        const VulkanBuffer& buffer,
        const VkExtent3D& extent,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t mipLevel);
    void buildMipmaps(VkCommandBuffer commandBuffer);
    void blit(VkCommandBuffer commandBuffer, const VulkanImage& image, uint32_t mipLevel);

    uint32_t getMipLevels() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    VkImageAspectFlags getAspectMask() const;
    VkFormat getFormat() const;
    uint32_t getLayerCount() const;

    VkImageSubresourceRange getFullRange() const;

    const VulkanDevice& getDevice() const;

    VkDeviceSize getSizeInBytes() const {
        return m_allocation.size;
    }

private:
    bool matchesLayout(VkImageLayout imageLayout, const VkImageSubresourceRange& range) const;
    bool isSameLayoutInRange(const VkImageSubresourceRange& range) const;
    std::pair<VkAccessFlags, VkAccessFlags> determineAccessMasks(VkImageLayout oldLayout, VkImageLayout newLayout) const;

    VulkanMemoryHeap::Allocation m_allocation;
    const VulkanDevice* m_device;
    VkImageCreateInfo m_createInfo;
    VkImageAspectFlags m_aspect;

    // Tracks layouts across all layers and mip levels.
    // Indexed as [layer][mipLevel].
    std::vector<std::vector<VkImageLayout>> m_layouts;
};

VkImageAspectFlags determineImageAspect(VkFormat format);

bool isDepthFormat(VkFormat format);

const char* toString(VkImageLayout layout);
std::string toString(VkImageUsageFlags flags);

} // namespace crisp