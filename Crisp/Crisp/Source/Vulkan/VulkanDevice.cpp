#include "VulkanDevice.hpp"

#include <iostream>

#include "VulkanContext.hpp"

namespace crisp
{
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
        MemoryHeap bufferHeap(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, m_context->findDeviceBufferMemoryType(m_device), m_device);
        m_memoryHeaps.insert(std::make_pair(bufferHeap.memoryTypeIndex, bufferHeap));

        // Device image memory
        MemoryHeap imageHeap(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, m_context->findDeviceImageMemoryType(m_device), m_device);
        m_memoryHeaps.insert(std::make_pair(imageHeap.memoryTypeIndex, imageHeap));

        // Staging memory
        MemoryHeap stagingHeap(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, StagingHeapSize, m_context->findStagingBufferMemoryType(m_device), m_device);
        m_memoryHeaps.insert(std::make_pair(stagingHeap.memoryTypeIndex, stagingHeap));

        // Command pool for the graphics queue family
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
    }

    VulkanDevice::~VulkanDevice()
    {
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

        for (auto& item : m_memoryHeaps)
            vkFreeMemory(m_device, item.second.memory, nullptr);
        m_memoryHeaps.clear();

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
        auto stagingBufferInfo = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data;
        vkMapMemory(m_device, stagingBufferInfo.second.memoryHeap->memory, stagingBufferInfo.second.offset, size, 0, &data);
        memcpy(data, srcData, static_cast<size_t>(size));
        vkUnmapMemory(m_device, stagingBufferInfo.second.memoryHeap->memory);

        copyBuffer(dstBuffer, stagingBufferInfo.first, 0, dstOffset, size);

        vkDestroyBuffer(m_device, stagingBufferInfo.first, nullptr);
        stagingBufferInfo.second.memoryHeap->free(stagingBufferInfo.second.offset, stagingBufferInfo.second.size);
    }

    VkBuffer VulkanDevice::createDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
    {
        auto bufferInfo = createBuffer(size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_deviceBuffers.insert(bufferInfo);

        return bufferInfo.first;
    }

    VkBuffer VulkanDevice::createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
    {
        auto bufferInfo = createBuffer(size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_stagingBuffers.insert(bufferInfo);

        return bufferInfo.first;
    }

    void VulkanDevice::updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize size)
    {
        updateDeviceBuffer(dstBuffer, stagingBuffer, srcData, 0, 0, size);
    }

    void VulkanDevice::updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize stagingOffset, VkDeviceSize dstOffset, VkDeviceSize size)
    {
        auto bufferChunk = m_stagingBuffers.at(stagingBuffer);

        void* data;
        vkMapMemory(m_device, bufferChunk.memoryHeap->memory, bufferChunk.offset + stagingOffset, size, 0, &data);
        memcpy(data, srcData, static_cast<size_t>(size));
        vkUnmapMemory(m_device, bufferChunk.memoryHeap->memory);

        copyBuffer(dstBuffer, stagingBuffer, stagingOffset, dstOffset, size);
    }

    void VulkanDevice::fillStagingBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size)
    {
        auto chunk = m_stagingBuffers.at(dstBuffer);

        void* data;
        vkMapMemory(m_device, chunk.memoryHeap->memory, chunk.offset, size, 0, &data);
        memcpy(data, srcData, static_cast<size_t>(size));
        vkUnmapMemory(m_device, chunk.memoryHeap->memory);
    }

    void* VulkanDevice::mapBuffer(VkBuffer stagingBuffer)
    {
        auto chunk = m_stagingBuffers.at(stagingBuffer);

        void* ptr;
        vkMapMemory(m_device, chunk.memoryHeap->memory, chunk.offset, chunk.size, 0, &ptr);
        return ptr;
    }

    void VulkanDevice::unmapBuffer(VkBuffer stagingBuffer)
    {
        auto chunk = m_stagingBuffers.at(stagingBuffer);
        vkUnmapMemory(m_device, chunk.memoryHeap->memory);
    }

    VkImage VulkanDevice::createDeviceImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage)
    {
        auto chunk = createImage(width, height, 1, format, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_deviceImages.insert(chunk);

        return chunk.first;
    }

    void VulkanDevice::fillDeviceImage(VkImage dstImage, const void* srcData, VkDeviceSize size, VkExtent3D extent, uint32_t numLayers)
    {
        auto stagingBufferInfo = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data;
        vkMapMemory(m_device, stagingBufferInfo.second.memoryHeap->memory, stagingBufferInfo.second.offset, size, 0, &data);
        memcpy(data, srcData, static_cast<size_t>(size));
        vkUnmapMemory(m_device, stagingBufferInfo.second.memoryHeap->memory);

        transitionImageLayout(dstImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numLayers);

        for (auto i = 0u; i < numLayers; i++)
            copyBufferToImage(dstImage, stagingBufferInfo.first, extent, i);

        vkDestroyBuffer(m_device, stagingBufferInfo.first, nullptr);
        stagingBufferInfo.second.memoryHeap->freeChunks[stagingBufferInfo.second.offset] = stagingBufferInfo.second.size;
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

    VkSemaphore VulkanDevice::createSemaphore()
    {
        VkSemaphoreCreateInfo semInfo = {};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        vkCreateSemaphore(m_device, &semInfo, nullptr, &semaphore);
        return semaphore;
    }

    std::pair<VkBuffer, MemoryChunk> VulkanDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
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
        if (m_memoryHeaps.find(supportedHeapIndex) == m_memoryHeaps.end())
            std::cerr << "Unable to find a suitable memory heap!";

        MemoryHeap& heap = m_memoryHeaps.at(supportedHeapIndex);
        if (heap.properties != props)
            std::cerr << "Buffer has requested unallocated memory type!\n";

        auto allocChunk = heap.allocateChunk(memRequirements.size, memRequirements.alignment);
        vkBindBufferMemory(m_device, buffer, heap.memory, allocChunk.offset);
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

    void VulkanDevice::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t numLayers)
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
        barrier.subresourceRange.baseArrayLayer = 0;
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
        else
        {
            std::cerr << "Unsupported layout transition!" << std::endl;
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    VkImage VulkanDevice::createDeviceImageArray(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageUsageFlags usage)
    {
        auto imageInfo = createImage(width, height, layers, format, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = extent.width;
        copyRegion.bufferImageHeight = extent.height;
        copyRegion.imageExtent = extent;
        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.baseArrayLayer = dstLayer;
        copyRegion.imageSubresource.layerCount = 1;
        vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    std::pair<VkImage, MemoryChunk> VulkanDevice::createImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps)
    {
        // Create an image handle
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = layers;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;

        VkImage image(VK_NULL_HANDLE);
        vkCreateImage(m_device, &imageInfo, nullptr, &image);

        // Assign the image to the proper memory heap
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);

        uint32_t supportedHeapIndex = m_context->findMemoryType(memRequirements.memoryTypeBits, memProps);
        if (m_memoryHeaps.find(supportedHeapIndex) == m_memoryHeaps.end())
            std::cerr << "Unable to find a suitable memory heap!";

        MemoryHeap& heap = m_memoryHeaps.at(supportedHeapIndex);
        if (heap.properties != memProps)
            std::cerr << "Image has requested unallocated memory type!\n";

        auto allocChunk = heap.allocateChunk(memRequirements.size, memRequirements.alignment);

        vkBindImageMemory(m_device, image, heap.memory, allocChunk.offset);

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
        for (auto& heap : m_memoryHeaps)
        {
            if (heap.first == 9) continue;
            std::cout << "Heap 1: " << heap.first << "\n";
            std::cout << "  Free chunks: \n";
            for (auto& chunk : heap.second.freeChunks)
                std::cout << "    " << chunk.first << " - " << chunk.second << "-" << chunk.first + chunk.second << "\n";
        }
    }

    std::pair<uint64_t, uint64_t> VulkanDevice::getDeviceMemoryUsage()
    {
        uint64_t allocatedSize = 0;
        uint64_t takenSize = 0;
        for (auto& heap : m_memoryHeaps)
        {
            if (heap.second.size == DeviceHeapSize)
            {
                allocatedSize += heap.second.size;
                takenSize += heap.second.usedSize;
            }
        }

        return { allocatedSize, takenSize };
    }
}
