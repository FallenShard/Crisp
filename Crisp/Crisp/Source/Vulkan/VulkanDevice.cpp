#include "VulkanDevice.hpp"

#include <iostream>

#include "VulkanContext.hpp"

namespace crisp
{
    MemoryHeap::MemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize allocSize, uint32_t memTypeIdx, VkDevice device)
        : memory(VK_NULL_HANDLE)
        , properties(memProps)
        , size(allocSize)
        , memoryTypeIndex(memTypeIdx)
    {
        VkMemoryAllocateInfo devAllocInfo = {};
        devAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        devAllocInfo.allocationSize = size;
        devAllocInfo.memoryTypeIndex = memoryTypeIndex;

        vkAllocateMemory(device, &devAllocInfo, nullptr, &memory);

        freeChunks.insert(std::make_pair(0, allocSize));
    }

    void MemoryHeap::free(uint64_t offset, uint64_t size)
    {
        freeChunks[offset] = size;

        uint64_t rightBound = offset + size;
        auto foundRight = freeChunks.find(rightBound);
        if (foundRight != freeChunks.end())
        {
            freeChunks[offset] = size + foundRight->second;
            freeChunks.erase(foundRight);
        }
    }

    void MemoryHeap::coalesce()
    {

    }

    std::pair<uint64_t, uint64_t> MemoryHeap::allocateChunk(uint64_t size, uint64_t alignment)
    {
        VkDeviceSize paddedSize = size + ((alignment - (size & (alignment - 1))) & (alignment - 1));

        std::pair<uint64_t, uint64_t> allocResult = {0, 0};
        uint64_t foundChunkOffset = 0;
        uint64_t foundChunkSize = 0;
        for (auto& freeChunk : freeChunks)
        {
            uint64_t chunkOffset = freeChunk.first;
            uint64_t chunkSize = freeChunk.second;

            uint64_t paddedOffset = chunkOffset + ((alignment - (chunkOffset & (alignment - 1))) & (alignment - 1));
            uint64_t usableSize = chunkSize - (paddedOffset - chunkOffset);

            if (paddedSize <= usableSize)
            {
                foundChunkOffset = chunkOffset;
                foundChunkSize = chunkSize;

                allocResult.first = paddedOffset;
                allocResult.second = paddedSize;

                break;
            }
        }

        if (foundChunkSize > 0)
        {
            // Erase the modified chunk
            freeChunks.erase(foundChunkOffset);

            // Possibly add the left chunk
            if (allocResult.first > foundChunkOffset)
                freeChunks[foundChunkOffset] = allocResult.first - foundChunkOffset;

            // Possibly add the right chunk
            if (foundChunkOffset + foundChunkSize > allocResult.first + allocResult.second)
                freeChunks[allocResult.first + allocResult.second] = foundChunkOffset + foundChunkSize - (allocResult.first + allocResult.second);
        }

        return allocResult;
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

        uint64_t allocationSize = 128 * 1024 * 1024; // 128 MB
        uint64_t stagingSize    =  32 * 1024 * 1024; //  32 MB

        // Device buffer memory
        MemoryHeap bufferHeap(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocationSize, m_context->findDeviceBufferMemoryType(m_device), m_device);
        m_memoryHeaps.insert(std::make_pair(bufferHeap.memoryTypeIndex, bufferHeap));

        // Device image memory
        MemoryHeap imageHeap(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocationSize, m_context->findDeviceImageMemoryType(m_device), m_device);
        m_memoryHeaps.insert(std::make_pair(imageHeap.memoryTypeIndex, imageHeap));

        // Staging memory
        MemoryHeap stagingHeap(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingSize, m_context->findStagingBufferMemoryType(m_device), m_device);
        m_memoryHeaps.insert(std::make_pair(stagingHeap.memoryTypeIndex, stagingHeap));

        // Command pool for the graphics queue family
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        poolInfo.flags = 0;

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
        auto imageInfo = m_deviceImages[image];
        imageInfo.memoryHeap->free(imageInfo.offset, imageInfo.size);

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

    void VulkanDevice::fillDeviceBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size)
    {
        auto stagingBufferInfo = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data;
        vkMapMemory(m_device, stagingBufferInfo.second.memoryHeap->memory, stagingBufferInfo.second.offset, size, 0, &data);
        memcpy(data, srcData, static_cast<size_t>(size));
        vkUnmapMemory(m_device, stagingBufferInfo.second.memoryHeap->memory);

        copyBuffer(dstBuffer, stagingBufferInfo.first, size);

        vkDestroyBuffer(m_device, stagingBufferInfo.first, nullptr);
        stagingBufferInfo.second.memoryHeap->free(stagingBufferInfo.second.offset, stagingBufferInfo.second.size);
    }

    VkBuffer VulkanDevice::createDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
    {
        auto bufferInfo = createBuffer(size, usage, props);
        m_deviceBuffers.insert(bufferInfo);

        return bufferInfo.first;
    }

    VkBuffer VulkanDevice::createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps)
    {
        auto bufferInfo = createBuffer(size, usage, memProps);
        m_stagingBuffers.insert(bufferInfo);

        return bufferInfo.first;
    }

    void VulkanDevice::updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize size)
    {
        auto stagingAlloc = m_stagingBuffers[stagingBuffer];

        void* data;
        vkMapMemory(m_device, stagingAlloc.memoryHeap->memory, stagingAlloc.offset, size, 0, &data);
        memcpy(data, srcData, static_cast<size_t>(size));
        vkUnmapMemory(m_device, stagingAlloc.memoryHeap->memory);

        copyBuffer(dstBuffer, stagingBuffer, size);
    }

    VkImage VulkanDevice::createDeviceImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps)
    {
        auto imageInfo = createImage(width, height, format, tiling, usage, memProps);
        m_deviceImages.insert(imageInfo);

        return imageInfo.first;
    }

    void VulkanDevice::fillDeviceImage(VkImage dstImage, const void* srcData, VkDeviceSize size, uint32_t width, uint32_t height)
    {
        auto stagingImageInfo = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data;
        vkMapMemory(m_device, stagingImageInfo.second.memoryHeap->memory, stagingImageInfo.second.offset, size, 0, &data);
        memcpy(data, srcData, static_cast<size_t>(size));
        vkUnmapMemory(m_device, stagingImageInfo.second.memoryHeap->memory);

        transitionImageLayout(stagingImageInfo.first, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transitionImageLayout(dstImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyImage(dstImage, stagingImageInfo.first, width, height);


        vkDestroyImage(m_device, stagingImageInfo.first, nullptr);
        stagingImageInfo.second.memoryHeap->freeChunks[stagingImageInfo.second.offset] = stagingImageInfo.second.size;
    }

    VkImageView VulkanDevice::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView(VK_NULL_HANDLE);
        vkCreateImageView(m_device, &viewInfo, nullptr, &imageView);
        return imageView;
    }

    std::pair<VkBuffer, AllocationDesc> VulkanDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
    {
        // Create a buffer handle
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
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
        if (heap.properties == props)
            std::cerr << "Buffer has requested unallocated memory type!";

        auto allocChunk = heap.allocateChunk(memRequirements.size, memRequirements.alignment);

        vkBindBufferMemory(m_device, buffer, heap.memory, allocChunk.first);

        AllocationDesc desc;
        desc.memoryHeap = &heap;
        desc.offset = allocChunk.first;
        desc.size = allocChunk.second;

        return std::make_pair(buffer, desc);
    }

    void VulkanDevice::copyBuffer(VkBuffer dstBuffer, VkBuffer srcBuffer, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    std::pair<VkImage, AllocationDesc> VulkanDevice::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps)
    {
        // Create an image handle
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
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
        if (heap.properties == memProps)
            std::cerr << "Image has requested unallocated memory type!";

        auto allocChunk = heap.allocateChunk(memRequirements.size, memRequirements.alignment);

        vkBindImageMemory(m_device, image, heap.memory, allocChunk.first);

        AllocationDesc desc;
        desc.memoryHeap = &heap;
        desc.offset = allocChunk.first;
        desc.size = allocChunk.second;

        return std::make_pair(image, desc);
    }

    void VulkanDevice::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

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

    void VulkanDevice::copyImage(VkImage dstImage, VkImage srcImage, uint32_t width, uint32_t height)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageSubresourceLayers subResource = {};
        subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subResource.baseArrayLayer = 0;
        subResource.mipLevel = 0;
        subResource.layerCount = 1;

        VkImageCopy region = {};
        region.srcSubresource = subResource;
        region.dstSubresource = subResource;
        region.srcOffset = {0, 0, 0};
        region.dstOffset = {0, 0, 0};
        region.extent.width = width;
        region.extent.height = height;
        region.extent.depth = 1;

        vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
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
}
