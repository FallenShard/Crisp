#pragma once

#include <vector>

#include "Vulkan/VulkanResource.hpp"
#include "Vulkan/VulkanMemoryHeap.hpp"

namespace crisp
{
    class VulkanDevice;
    class VulkanBuffer;

    class VulkanImage : public VulkanResource<VkImage>
    {
    public:
        VulkanImage(VulkanDevice* device, VkExtent3D extent, uint32_t numLayers, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags);
        ~VulkanImage();

        void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        void transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer);
        void copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers);

        VkImageView createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers) const;

    private:
        VulkanMemoryChunk m_memoryChunk;

        VkFormat   m_format;
        VkExtent3D m_extent;
        uint32_t   m_numLayers;
        uint32_t   m_mipLevels;
        std::vector<VkImageLayout> m_layouts;
        VkImageAspectFlags m_aspectMask;
    };
}