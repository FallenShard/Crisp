#pragma once

#include <memory>
#include <vector>

#include "Vulkan/VulkanResource.hpp"
#include "Vulkan/VulkanMemoryHeap.hpp"

namespace crisp
{
    class VulkanDevice;
    class VulkanBuffer;
    class VulkanImageView;

    class VulkanImage : public VulkanResource<VkImage>
    {
    public:
        VulkanImage(VulkanDevice* device, VkExtent3D extent, uint32_t numLayers, uint32_t numMipmaps, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags);
        ~VulkanImage();

        void setImageLayout(VkImageLayout newLayout, uint32_t baseLayer);
        void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, VkImageSubresourceRange subresRange, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

        void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer);
        void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers);

        std::unique_ptr<VulkanImageView> createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel = 0, uint32_t mipLevels = 1) const;

        uint32_t getMipLevels() const;
        uint32_t getWidth() const;
        uint32_t getHeight() const;
        VkImageAspectFlags getAspectMask() const;
        VkFormat getFormat() const;

    private:
        std::pair<VkAccessFlags, VkAccessFlags> determineAccessMasks(VkImageLayout oldLayout, VkImageLayout newLayout) const;

        VulkanMemoryChunk m_memoryChunk;

        VkFormat   m_format;
        VkExtent3D m_extent;
        uint32_t   m_numLayers;
        uint32_t   m_mipLevels;
        std::vector<std::vector<VkImageLayout>> m_layouts;
        VkImageAspectFlags m_aspectMask;
    };
}