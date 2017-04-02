#include "VulkanDevice.hpp"

#include <iostream>

#include "VulkanContext.hpp"

namespace crisp
{
    namespace
    {
        VkMemoryPropertyFlags stagingMemoryBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        std::vector<VkMappedMemoryRange> unflushedRanges;
    }

    VulkanDevice::VulkanDevice(VulkanContext* vulkanContext)
        : m_context(vulkanContext)
        , m_device(VK_NULL_HANDLE)
        , m_graphicsQueue(VK_NULL_HANDLE)
        , m_presentQueue(VK_NULL_HANDLE)
        , m_commandPool(VK_NULL_HANDLE)
    {
        m_device = m_context->createLogicalDevice();
        auto queueFamilyIndices = m_context->findQueueFamilies();
        vkGetDeviceQueue(m_device, queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, queueFamilyIndices.presentFamily,  0, &m_presentQueue);

        // Device buffer memory
        m_deviceBufferHeap = std::make_unique<MemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, m_context->findDeviceBufferMemoryType(m_device), m_device, "Device Buffer Heap");

        // Device image memory
        m_deviceImageHeap = std::make_unique<MemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 2 * DeviceHeapSize, m_context->findDeviceImageMemoryType(m_device), m_device, "Device Image Heap");

        // Staging memory
        m_stagingBufferHeap = std::make_unique<MemoryHeap>(stagingMemoryBits, StagingHeapSize, m_context->findStagingBufferMemoryType(m_device), m_device, "Staging Buffer Heap");
        vkMapMemory(m_device, m_stagingBufferHeap->memory, 0, m_stagingBufferHeap->size, 0, &m_mappedStagingPtr);

        // Command pool for the graphics queue family
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
    }

    VulkanDevice::~VulkanDevice()
    {
        vkUnmapMemory(m_device, m_stagingBufferHeap->memory);
        freeResources();
    }

    void VulkanDevice::freeResources()
    {
        for (auto& buffer : m_deviceBuffers)
            vkDestroyBuffer(m_device, buffer.first, nullptr);
        m_deviceBuffers.clear();

        for (auto& buffer : m_stagingBuffers)
            vkDestroyBuffer(m_device, buffer.first, nullptr);
        m_stagingBuffers.clear();

        for (auto& image : m_deviceImages)
            vkDestroyImage(m_device, image.first, nullptr);
        m_deviceImages.clear();

        vkFreeMemory(m_device, m_deviceBufferHeap->memory, nullptr);
        vkFreeMemory(m_device, m_deviceImageHeap->memory, nullptr);
        vkFreeMemory(m_device, m_stagingBufferHeap->memory, nullptr);

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        vkDestroyDevice(m_device, nullptr);
    }

    void VulkanDevice::destroyDeviceImage(VkImage image)
    {
        auto memoryChunk = m_deviceImages.at(image);
        memoryChunk.memoryHeap->free(memoryChunk.offset, memoryChunk.size);

        vkDestroyImage(m_device, image, nullptr);
        m_deviceImages.erase(image);
    }

    void VulkanDevice::destroyDeviceBuffer(VkBuffer buffer)
    {
        auto memChunk = m_deviceBuffers.at(buffer);
        memChunk.memoryHeap->free(memChunk.offset, memChunk.size);

        vkDestroyBuffer(m_device, buffer, nullptr);
        m_deviceBuffers.erase(buffer);
    }

    void VulkanDevice::destroyStagingBuffer(VkBuffer buffer)
    {
        auto memChunk = m_stagingBuffers.at(buffer);
        memChunk.memoryHeap->free(memChunk.offset, memChunk.size);

        vkDestroyBuffer(m_device, buffer, nullptr);
        m_stagingBuffers.erase(buffer);
    }

    VkDevice VulkanDevice::getHandle() const
    {
        return m_device;
    }

    VkQueue VulkanDevice::getGraphicsQueue() const
    {
        return m_graphicsQueue;
    }

    VkQueue VulkanDevice::getPresentQueue() const
    {
        return m_presentQueue;
    }

    VkCommandPool VulkanDevice::getCommandPool() const
    {
        return m_commandPool;
    }

    MemoryChunk VulkanDevice::getStagingBufferChunk(VkBuffer buffer)
    {
        return m_stagingBuffers.at(buffer);
    }

    void VulkanDevice::fillDeviceBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size)
    {
        fillDeviceBuffer(dstBuffer, srcData, 0, size);
    }

    void VulkanDevice::fillDeviceBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize dstOffset, VkDeviceSize size)
    {
        auto stagingBufferInfo = createBuffer(m_stagingBufferHeap.get(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingMemoryBits);
        memcpy(static_cast<char*>(m_mappedStagingPtr) + stagingBufferInfo.second.offset, srcData, static_cast<size_t>(size));
        copyBuffer(dstBuffer, stagingBufferInfo.first, 0, dstOffset, size);

        vkDestroyBuffer(m_device, stagingBufferInfo.first, nullptr);
        stagingBufferInfo.second.memoryHeap->free(stagingBufferInfo.second.offset, stagingBufferInfo.second.size);
    }

    VkBuffer VulkanDevice::createDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* srcData)
    {
        auto bufferInfo = createBuffer(m_deviceBufferHeap.get(), size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_deviceBuffers.insert(bufferInfo);

        if (srcData != nullptr)
        {
            fillDeviceBuffer(bufferInfo.first, srcData, 0, size);
        }

        return bufferInfo.first;
    }

    VkBuffer VulkanDevice::createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
    {
        auto bufferInfo = createBuffer(m_stagingBufferHeap.get(), size, usage, stagingMemoryBits);
        m_stagingBuffers.insert(bufferInfo);

        return bufferInfo.first;
    }

    VkBuffer VulkanDevice::createStagingBuffer(VkBuffer srcBuffer, VkDeviceSize srcSize, VkDeviceSize newSize, VkBufferUsageFlags usage)
    {
        // Create new staging buffer
        auto bufferInfo = createBuffer(m_stagingBufferHeap.get(), newSize, usage, stagingMemoryBits);
        m_stagingBuffers.insert(bufferInfo);

        // Get memory of the previous buffer and copy into new one
        auto srcChunk = m_stagingBuffers.at(srcBuffer);
        memcpy(static_cast<char*>(m_mappedStagingPtr) + bufferInfo.second.offset, static_cast<char*>(m_mappedStagingPtr) + srcChunk.offset, srcSize);

        // return the new buffer
        return bufferInfo.first;
    }

    void VulkanDevice::updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize size)
    {
        updateDeviceBuffer(dstBuffer, stagingBuffer, srcData, 0, 0, size);
    }

    void VulkanDevice::updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize stagingOffset, VkDeviceSize dstOffset, VkDeviceSize size)
    {
        auto bufferChunk = m_stagingBuffers.at(stagingBuffer);
        memcpy(static_cast<char*>(m_mappedStagingPtr) + bufferChunk.offset + stagingOffset, srcData, static_cast<size_t>(size));
        copyBuffer(dstBuffer, stagingBuffer, stagingOffset, dstOffset, size);
    }

    void VulkanDevice::updateStagingBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize offset, VkDeviceSize size)
    {
        auto chunk = m_stagingBuffers.at(dstBuffer);
        memcpy(static_cast<char*>(m_mappedStagingPtr) + chunk.offset + offset, srcData, static_cast<size_t>(size));
    }

    VkBuffer VulkanDevice::resizeStagingBuffer(VkBuffer prevBuffer, VkDeviceSize prevSize, VkDeviceSize newSize, VkBufferUsageFlags usage)
    {
        // Create new staging buffer
        auto bufferInfo = createBuffer(m_stagingBufferHeap.get(), newSize, usage, stagingMemoryBits);
        m_stagingBuffers.insert(bufferInfo);

        // Get memory of the previous buffer and copy into new one
        auto srcChunk = m_stagingBuffers.at(prevBuffer);
        memcpy(static_cast<char*>(m_mappedStagingPtr) + bufferInfo.second.offset, static_cast<char*>(m_mappedStagingPtr) + srcChunk.offset, prevSize);

        // Delete the old buffer
        destroyStagingBuffer(prevBuffer);

        // return the new buffer
        return bufferInfo.first;
    }

    void VulkanDevice::fillStagingBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size)
    {
        auto chunk = m_stagingBuffers.at(dstBuffer);
        memcpy(static_cast<char*>(m_mappedStagingPtr) + chunk.offset, srcData, static_cast<size_t>(size));
    }

    void VulkanDevice::flushMappedRanges()
    {
        vkFlushMappedMemoryRanges(m_device, static_cast<uint32_t>(unflushedRanges.size()), unflushedRanges.data());
        unflushedRanges.clear();
    }

    VkImage VulkanDevice::createDeviceImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage)
    {
        auto chunk = createImage(m_deviceImageHeap.get(), { width, height, 1u }, 1, format, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_deviceImages.insert(chunk);

        return chunk.first;
    }

    void VulkanDevice::fillDeviceImage(VkImage dstImage, const void* srcData, VkDeviceSize size, VkExtent3D extent, uint32_t numLayers, uint32_t firstLayer)
    {
        auto stagingBufferInfo = createBuffer(m_stagingBufferHeap.get(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingMemoryBits);

        memcpy(static_cast<char*>(m_mappedStagingPtr) + stagingBufferInfo.second.offset, srcData, static_cast<size_t>(size));

        transitionImageLayout(dstImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numLayers, firstLayer);

        for (auto i = firstLayer; i < firstLayer + numLayers; i++)
            copyBufferToImage(dstImage, stagingBufferInfo.first, extent, i);

        vkDestroyBuffer(m_device, stagingBufferInfo.first, nullptr);
        stagingBufferInfo.second.memoryHeap->free(stagingBufferInfo.second);
    }

    void VulkanDevice::updateDeviceImage(VkImage dstImage, VkBuffer stagingBuffer, VkDeviceSize size, VkExtent3D extent, uint32_t numLayers)
    {
        transitionImageLayout(dstImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numLayers);

        for (auto i = 0u; i < numLayers; i++)
            copyBufferToImage(dstImage, stagingBuffer, extent, i);
    }

    VkImageView VulkanDevice::createImageView(VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect, uint32_t baseLayer, uint32_t numLayers) const
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image        = image;
        viewInfo.viewType     = viewType;
        viewInfo.format       = format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = aspect;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = baseLayer;
        viewInfo.subresourceRange.layerCount     = numLayers;

        VkImageView imageView(VK_NULL_HANDLE);
        vkCreateImageView(m_device, &viewInfo, nullptr, &imageView);
        
        return imageView;
    }

    VkSampler VulkanDevice::createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode)
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = magFilter;
        samplerInfo.minFilter               = minFilter;
        samplerInfo.addressModeU            = addressMode;
        samplerInfo.addressModeV            = addressMode;
        samplerInfo.addressModeW            = addressMode;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 0.0f;

        VkSampler sampler = VK_NULL_HANDLE;
        vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler);
        return sampler;
    }

    VkSemaphore VulkanDevice::createSemaphore()
    {
        VkSemaphoreCreateInfo semInfo = {};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        vkCreateSemaphore(m_device, &semInfo, nullptr, &semaphore);
        return semaphore;
    }

    MemoryHeap* VulkanDevice::getDeviceBufferHeap() const
    {
        return m_deviceBufferHeap.get();
    }

    MemoryHeap* VulkanDevice::getStagingBufferHeap() const
    {
        return m_stagingBufferHeap.get();
    }

    void* VulkanDevice::getStagingMemoryPtr() const
    {
        return m_mappedStagingPtr;
    }

    std::pair<VkBuffer, MemoryChunk> VulkanDevice::createBuffer(MemoryHeap* heap, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
    {
        // Create a buffer handle
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size        = size;
        bufferInfo.usage       = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer(VK_NULL_HANDLE);
        vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer);

        // Assign the buffer to a suitable memory heap by giving it a free chunk
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

        uint32_t supportedHeapIndex = m_context->findMemoryType(memRequirements.memoryTypeBits, props);
        if (supportedHeapIndex != heap->memoryTypeIndex)
            std::cerr << "Wrong heap type specified when creating buffer!";

        if (heap->properties != props)
            std::cerr << "Buffer has requested unallocated memory type!\n";

        auto allocChunk = heap->allocateChunk(memRequirements.size, memRequirements.alignment);
        vkBindBufferMemory(m_device, buffer, heap->memory, allocChunk.offset);
        return std::make_pair(buffer, allocChunk);
    }

    void VulkanDevice::copyBuffer(VkBuffer dstBuffer, VkBuffer srcBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size      = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanDevice::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t numLayers, uint32_t baseLayer)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = baseLayer;
        barrier.subresourceRange.layerCount     = numLayers;
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

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
            std::cerr << "Unsupported layout transition!" << std::endl;
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    VkImage VulkanDevice::createDeviceImageArray(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageCreateFlags createFlags)
    {
        auto imageInfo = createImage(m_deviceImageHeap.get(), { width, height, 1u }, layers, format, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, createFlags);
        m_deviceImages.insert(imageInfo);

        return imageInfo.first;
    }

    void VulkanDevice::copyImage(VkImage dstImage, VkImage srcImage, VkExtent3D extent, uint32_t dstLayer)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageCopy region = {};
        region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.mipLevel       = 0;
        region.srcSubresource.layerCount     = 1;
        region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer = dstLayer;
        region.dstSubresource.mipLevel       = 0;
        region.dstSubresource.layerCount     = 1;
        region.srcOffset = {0, 0, 0};
        region.dstOffset = {0, 0, 0};
        region.extent    = extent;

        vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanDevice::copyBufferToImage(VkImage dstImage, VkBuffer srcBuffer, VkExtent3D extent, uint32_t dstLayer)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset                    = 0;
        copyRegion.bufferRowLength                 = extent.width;
        copyRegion.bufferImageHeight               = extent.height;
        copyRegion.imageExtent                     = extent;
        copyRegion.imageOffset                     = { 0, 0, 0 };
        copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.baseArrayLayer = dstLayer;
        copyRegion.imageSubresource.layerCount     = 1;
        vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    std::pair<VkImage, MemoryChunk> VulkanDevice::createImage(MemoryHeap* heap, VkExtent3D extent, uint32_t layers, VkFormat format, VkImageLayout initLayout, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps, VkImageCreateFlags createFlags)
    {
        // Create an image handle
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent        = extent;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = layers;
        imageInfo.format        = format;
        imageInfo.tiling        = tiling;
        imageInfo.initialLayout = initLayout;
        imageInfo.usage         = usage;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags         = 0;

        VkImage image(VK_NULL_HANDLE);
        auto res = vkCreateImage(m_device, &imageInfo, nullptr, &image);

        if (res != VK_SUCCESS)
        {
            std::cerr << "FAILED TO CREATE IMAGE!" << std::endl;
        }

        // Assign the image to the proper memory heap
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);

        uint32_t supportedHeapIndex = m_context->findMemoryType(memRequirements.memoryTypeBits, memProps);
        if (supportedHeapIndex != heap->memoryTypeIndex)
            std::cerr << "Tried to create an image with wrong heap type!\n";

        if (heap->properties != memProps)
            std::cerr << "Image has requested unallocated memory type!\n";

        auto allocChunk = heap->allocateChunk(memRequirements.size, memRequirements.alignment);

        res = vkBindImageMemory(m_device, image, heap->memory, allocChunk.offset);
        if (res != VK_SUCCESS)
        {
            std::cerr << "FAILED TO BIND IMAGE!" << std::endl;
        }

        return std::make_pair(image, allocChunk);
    }

    VkCommandBuffer VulkanDevice::beginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void VulkanDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphicsQueue);

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    }

    void VulkanDevice::printMemoryStatus()
    {
        auto printSingleHeap = [](MemoryHeap* heap)
        {
            std::cout << heap->tag << "\n";
            std::cout << "  Free chunks: \n";
            for (auto& chunk : heap->freeChunks)
                std::cout << "    " << chunk.first << " - " << chunk.second << "-" << chunk.first + chunk.second << "\n";
        };

        printSingleHeap(m_deviceBufferHeap.get());
        printSingleHeap(m_deviceImageHeap.get());
        printSingleHeap(m_stagingBufferHeap.get());
    }

    DeviceMemoryMetrics VulkanDevice::getDeviceMemoryUsage()
    {
        DeviceMemoryMetrics memoryMetrics = {};
        memoryMetrics.bufferMemorySize  = m_deviceBufferHeap->size;
        memoryMetrics.bufferMemoryUsed  = m_deviceBufferHeap->usedSize;
        memoryMetrics.imageMemorySize   = m_deviceImageHeap->size;
        memoryMetrics.imageMemoryUsed   = m_deviceImageHeap->usedSize;
        memoryMetrics.stagingMemorySize = m_stagingBufferHeap->size;
        memoryMetrics.stagingMemoryUsed = m_stagingBufferHeap->usedSize;
        return memoryMetrics;
    }
}
