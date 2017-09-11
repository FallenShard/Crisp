#include "VulkanImage.hpp"

#include <iostream>

#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"

namespace crisp
{
    VulkanImage::VulkanImage(VulkanDevice* device, VkExtent3D extent, uint32_t numLayers, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags)
        : VulkanResource(device)
        , m_format(format)
        , m_extent(extent)
        , m_numLayers(numLayers)
        , m_mipLevels(1)
        , m_layouts(numLayers, VK_IMAGE_LAYOUT_UNDEFINED)
        , m_aspectMask(aspect)
    {
        // Create an image handle
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.flags         = createFlags;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = format;
        imageInfo.extent        = extent;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = numLayers;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = usage;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkCreateImage(m_device->getHandle(), &imageInfo, nullptr, &m_handle);

        // Assign the image to the proper memory heap
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->getHandle(), m_handle, &memRequirements);

        auto heap = m_device->getDeviceImageHeap();
        m_memoryChunk = heap->allocateChunk(memRequirements.size, memRequirements.alignment);

        vkBindImageMemory(m_device->getHandle(), m_handle, heap->memory, m_memoryChunk.offset);
    }

    VulkanImage::~VulkanImage()
    {
        m_memoryChunk.free();
        vkDestroyImage(m_device->getHandle(), m_handle, nullptr);
    }

    void VulkanImage::transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        transitionLayout(cmdBuffer, newLayout, 0, m_numLayers, srcStage, dstStage);
    }

    void VulkanImage::transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        auto oldLayout = m_layouts[baseLayer];

        VkImageMemoryBarrier barrier = {};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_handle;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = baseLayer;
        barrier.subresourceRange.layerCount     = numLayers;
        barrier.subresourceRange.aspectMask     = m_aspectMask;

        if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }
        else
        {
            std::cerr << "Unsupported layout transition!\n";
        }

        vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        for (auto i = baseLayer; i < baseLayer + numLayers; ++i)
            m_layouts[i] = newLayout;
    }

    void VulkanImage::copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer)
    {
        copyFrom(commandBuffer, buffer, 0, m_numLayers);
    }

    void VulkanImage::copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers)
    {
        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset                    = 0;
        copyRegion.bufferImageHeight               = m_extent.height;
        copyRegion.bufferRowLength                 = m_extent.width;
        copyRegion.imageExtent                     = m_extent;
        copyRegion.imageOffset                     = { 0, 0, 0 };
        copyRegion.imageSubresource.aspectMask     = m_aspectMask;
        copyRegion.imageSubresource.baseArrayLayer = baseLayer;
        copyRegion.imageSubresource.layerCount     = numLayers;
        copyRegion.imageSubresource.mipLevel       = 0;
        vkCmdCopyBufferToImage(commandBuffer, buffer.getHandle(), m_handle, m_layouts[baseLayer], 1, &copyRegion);
    }

    VkImageView VulkanImage::createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers) const
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image        = m_handle;
        viewInfo.viewType     = type;
        viewInfo.format       = m_format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = m_aspectMask;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = baseLayer;
        viewInfo.subresourceRange.layerCount     = numLayers;

        VkImageView imageView(VK_NULL_HANDLE);
        vkCreateImageView(m_device->getHandle(), &viewInfo, nullptr, &imageView);
        return imageView;
    }
}