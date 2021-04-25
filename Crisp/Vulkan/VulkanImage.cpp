#include "VulkanImage.hpp"

#include <iostream>

#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanImageView.hpp"

#include <spdlog/spdlog.h>

namespace crisp
{
    namespace {
        uint32_t id = 0;
    }

    VulkanImage::VulkanImage(VulkanDevice* device, const VkImageCreateInfo& createInfo, VkImageAspectFlags aspect)
        : VulkanResource(device)
        , m_type(createInfo.imageType)
        , m_format(createInfo.format)
        , m_extent(createInfo.extent)
        , m_mipLevels(createInfo.mipLevels)
        , m_numLayers(createInfo.arrayLayers)
        , m_aspect(aspect)
        , m_layouts(m_numLayers, std::vector<VkImageLayout>(m_mipLevels, VK_IMAGE_LAYOUT_UNDEFINED))
    {
        vkCreateImage(m_device->getHandle(), &createInfo, nullptr, &m_handle);

        // Assign the image to the proper memory heap
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->getHandle(), m_handle, &memRequirements);
        m_memoryChunk = m_device->getMemoryAllocator()->getDeviceImageHeap()->allocate(memRequirements.size, memRequirements.alignment);
        vkBindImageMemory(m_device->getHandle(), m_handle, m_memoryChunk.getMemory(), m_memoryChunk.offset);
    }

    VulkanImage::VulkanImage(VulkanDevice* device, VkExtent3D extent, uint32_t numLayers, uint32_t numMipmaps, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags createFlags)
        : VulkanResource(device)
        , m_type(VK_IMAGE_TYPE_2D)
        , m_format(format)
        , m_extent(extent)
        , m_numLayers(numLayers)
        , m_mipLevels(numMipmaps)
        , m_aspect(aspect)
        , m_layouts(numLayers, std::vector<VkImageLayout>(numMipmaps, VK_IMAGE_LAYOUT_UNDEFINED))
    {
        // Create an image handle
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.flags         = createFlags;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = format;
        imageInfo.extent        = extent;
        imageInfo.mipLevels     = numMipmaps;
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
        m_memoryChunk = m_device->getMemoryAllocator()->getDeviceImageHeap()->allocate(memRequirements.size, memRequirements.alignment);
        vkBindImageMemory(m_device->getHandle(), m_handle, m_memoryChunk.getMemory(), m_memoryChunk.offset);
    }

    VulkanImage::~VulkanImage()
    {
        m_device->deferMemoryChunk(m_framesToLive, m_memoryChunk);
        m_device->deferDestruction(m_framesToLive, m_handle, [](void* handle, VkDevice device)
        {
            vkDestroyImage(device, static_cast<VkImage>(handle), nullptr);
        });
    }

    void VulkanImage::setImageLayout(VkImageLayout newLayout, uint32_t baseLayer)
    {
        m_layouts[baseLayer][0] = newLayout;
    }

    void VulkanImage::setImageLayout(VkImageLayout newLayout, VkImageSubresourceRange subresourceRange)
    {
        for (uint32_t i = subresourceRange.baseArrayLayer; i < subresourceRange.baseArrayLayer + subresourceRange.layerCount; ++i)
            for (uint32_t j = subresourceRange.baseMipLevel; j < subresourceRange.baseMipLevel + subresourceRange.levelCount; ++j)
                m_layouts[i][j] = newLayout;
    }

    void VulkanImage::transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        VkImageSubresourceRange subresRange;
        subresRange.baseMipLevel   = 0;
        subresRange.levelCount     = m_mipLevels;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount     = m_numLayers;
        subresRange.aspectMask     = m_aspect;
        transitionLayout(cmdBuffer, newLayout, subresRange, srcStage, dstStage);
    }

    void VulkanImage::transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        baseLayer = std::min(baseLayer, static_cast<uint32_t>(m_layouts.size()) - 1);
        auto oldLayout = m_layouts[baseLayer][0];
        if (oldLayout == newLayout)
            return;

        VkImageSubresourceRange subresRange;
        subresRange.baseMipLevel   = 0;
        subresRange.levelCount     = m_mipLevels;
        subresRange.baseArrayLayer = baseLayer;
        subresRange.layerCount     = numLayers;
        subresRange.aspectMask     = m_aspect;
        transitionLayout(cmdBuffer, newLayout, subresRange, srcStage, dstStage);
    }

    void VulkanImage::transitionLayout(VkCommandBuffer buffer, VkImageLayout newLayout, uint32_t baseLayer, uint32_t numLayers, uint32_t baseLevel, uint32_t levelCount, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        VkImageSubresourceRange subresRange;
        subresRange.baseMipLevel   = baseLevel;
        subresRange.levelCount     = levelCount;
        subresRange.baseArrayLayer = baseLayer;
        subresRange.layerCount     = numLayers;
        subresRange.aspectMask     = m_aspect;
        transitionLayout(buffer, newLayout, subresRange, srcStage, dstStage);
    }

    void VulkanImage::transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkImageSubresourceRange subresRange, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        auto oldLayout = m_layouts[subresRange.baseArrayLayer][subresRange.baseMipLevel];
        if (oldLayout == newLayout)
            return;

        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_handle;
        barrier.subresourceRange    = subresRange;
        std::tie(barrier.srcAccessMask, barrier.dstAccessMask) = determineAccessMasks(oldLayout, newLayout);

        vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        for (auto i = subresRange.baseArrayLayer; i < subresRange.baseArrayLayer + subresRange.layerCount; ++i) {
            for (auto j = subresRange.baseMipLevel; j < subresRange.baseMipLevel + subresRange.levelCount; ++j)
                m_layouts[i][j] = newLayout;
        }
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
        copyRegion.imageSubresource.aspectMask     = m_aspect;
        copyRegion.imageSubresource.baseArrayLayer = baseLayer;
        copyRegion.imageSubresource.layerCount     = numLayers;
        copyRegion.imageSubresource.mipLevel       = 0;
        vkCmdCopyBufferToImage(commandBuffer, buffer.getHandle(), m_handle, m_layouts[baseLayer][0], 1, &copyRegion);
    }

    void VulkanImage::buildMipmaps(VkCommandBuffer commandBuffer)
    {
        if (m_mipLevels > 1)
        {
            VkImageSubresourceRange subresRange = {};
            subresRange.aspectMask     = m_aspect;
            subresRange.baseMipLevel   = 0;
            subresRange.levelCount     = 1;
            subresRange.baseArrayLayer = 0;
            subresRange.layerCount     = m_numLayers;

            transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            for (uint32_t i = 1; i < m_mipLevels; i++)
            {
                VkImageBlit imageBlit = {};

                imageBlit.srcSubresource.aspectMask     = m_aspect;
                imageBlit.srcSubresource.baseArrayLayer = 0;
                imageBlit.srcSubresource.layerCount     = m_numLayers;
                imageBlit.srcSubresource.mipLevel       = i - 1;
                imageBlit.srcOffsets[1].x = std::max(int32_t(m_extent.width  >> (i - 1)), 1);
                imageBlit.srcOffsets[1].y = std::max(int32_t(m_extent.height >> (i - 1)), 1);
                imageBlit.srcOffsets[1].z = 1;

                imageBlit.dstSubresource.aspectMask     = m_aspect;
                imageBlit.dstSubresource.baseArrayLayer = 0;
                imageBlit.dstSubresource.layerCount     = m_numLayers;
                imageBlit.dstSubresource.mipLevel       = i;
                imageBlit.dstOffsets[1].x = std::max(int32_t(m_extent.width  >> i), 1);
                imageBlit.dstOffsets[1].y = std::max(int32_t(m_extent.height >> i), 1);
                imageBlit.dstOffsets[1].z = 1;

                subresRange.aspectMask     = m_aspect;
                subresRange.baseMipLevel   = i;
                subresRange.levelCount     = 1;
                subresRange.baseArrayLayer = 0;
                subresRange.layerCount     = m_numLayers;

                transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                vkCmdBlitImage(commandBuffer, m_handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
                transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            }
        }
    }

    void VulkanImage::blit(VkCommandBuffer commandBuffer, const VulkanImage& image, uint32_t mipLevel)
    {
        VkImageBlit imageBlit = {};
        imageBlit.srcSubresource.aspectMask     = m_aspect;
        imageBlit.srcSubresource.baseArrayLayer = 0;
        imageBlit.srcSubresource.layerCount     = 6;
        imageBlit.srcSubresource.mipLevel       = 0;
        imageBlit.srcOffsets[1].x = image.m_extent.width;
        imageBlit.srcOffsets[1].y = image.m_extent.height;
        imageBlit.srcOffsets[1].z = 1;

        imageBlit.dstSubresource.aspectMask     = m_aspect;
        imageBlit.dstSubresource.baseArrayLayer = 0;
        imageBlit.dstSubresource.layerCount     = 6;
        imageBlit.dstSubresource.mipLevel       = mipLevel;
        imageBlit.dstOffsets[1].x = image.m_extent.width;
        imageBlit.dstOffsets[1].y = image.m_extent.height;
        imageBlit.dstOffsets[1].z = 1;

        VkImageSubresourceRange mipRange = {};
        mipRange.aspectMask     = m_aspect;
        mipRange.baseMipLevel   = mipLevel;
        mipRange.levelCount     = 1;
        mipRange.baseArrayLayer = 0;
        mipRange.layerCount     = 6;

        transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        vkCmdBlitImage(commandBuffer, image.getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
        transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    std::unique_ptr<VulkanImageView> VulkanImage::createView(VkImageViewType type) const
    {
        return std::make_unique<VulkanImageView>(m_device, *this, type, 0, m_numLayers, 0, m_mipLevels);
    }

    std::unique_ptr<VulkanImageView> VulkanImage::createView(VkImageViewType type, uint32_t baseLayer, uint32_t numLayers, uint32_t baseMipLevel, uint32_t mipLevels) const
    {
        return std::make_unique<VulkanImageView>(m_device, *this, type, baseLayer, numLayers, baseMipLevel, mipLevels);
    }

    uint32_t VulkanImage::getMipLevels() const
    {
        return m_mipLevels;
    }

    uint32_t VulkanImage::getWidth() const
    {
        return m_extent.width;
    }

    uint32_t VulkanImage::getHeight() const
    {
        return m_extent.height;
    }

    VkImageAspectFlags VulkanImage::getAspectMask() const
    {
        return m_aspect;
    }

    VkFormat VulkanImage::getFormat() const
    {
        return m_format;
    }

    std::pair<VkAccessFlags, VkAccessFlags> VulkanImage::determineAccessMasks(VkImageLayout oldLayout, VkImageLayout newLayout) const
    {
        switch (oldLayout)
        {
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return { VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return { VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT };
            }

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return { VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:     return { VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT };
            case VK_IMAGE_LAYOUT_GENERAL:                  return { VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
            }

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return { VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT };
            }

        case VK_IMAGE_LAYOUT_UNDEFINED:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:             return { 0, VK_ACCESS_TRANSFER_WRITE_BIT };
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return { 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:         return { 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:         return { 0, VK_ACCESS_SHADER_READ_BIT };
            case VK_IMAGE_LAYOUT_GENERAL:                          return { 0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT };
            }

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return { VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return { VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT };
            case VK_IMAGE_LAYOUT_GENERAL:              return { VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT };
            }

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return { VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:     return { VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT };
            }

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return { VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
            }

        case VK_IMAGE_LAYOUT_GENERAL:
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return { VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:     return { VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT };
            }
        }

        spdlog::error("Unsupported layout transition: {} to {}!", oldLayout, newLayout);
        return { 0, 0 };
    }
}