#pragma once

#include <map>
#include <set>

#include <vulkan/vulkan.h>
#include "MemoryHeap.hpp"

namespace crisp
{
    class VulkanContext;

    class VulkanDevice
    {
    public:
        VulkanDevice(VulkanContext* vulkanContext);
        ~VulkanDevice();

        VkDevice getHandle() const;
        VkQueue getGraphicsQueue() const;
        VkQueue getPresentQueue() const;
        VkCommandPool getCommandPool() const;

        MemoryChunk getStagingBufferChunk(VkBuffer buffer);
        void fillDeviceBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size);
        void fillDeviceBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize dstOffset, VkDeviceSize size);
        VkBuffer createDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
        VkBuffer createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usages);
        void updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize size);
        void updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize stagingOffset, VkDeviceSize dstOffset, VkDeviceSize size);
        void fillStagingBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size);
        void* mapBuffer(VkBuffer stagingBuffer);
        void unmapBuffer(VkBuffer stagingBuffer);

        VkImage createDeviceImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage);
        void fillDeviceImage(VkImage dstImage, const void* srcData, VkDeviceSize size, VkExtent3D extent, uint32_t numLayers);
        void updateDeviceImage(VkImage dstImage, VkBuffer stagingBuffer, VkDeviceSize size, VkExtent3D extent, uint32_t numLayers);
        void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t numLayers);

        VkImage createDeviceImageArray(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageUsageFlags usage);

        VkImageView createImageView(VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect, uint32_t baseLayer, uint32_t numLayers) const;

        VkSemaphore createSemaphore();

        void freeResources();
        void destroyDeviceImage(VkImage image);

        void printMemoryStatus();

    private:
        std::pair<VkBuffer, MemoryChunk> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
        void copyBuffer(VkBuffer dstBuffer, VkBuffer srcBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);

        void copyImage(VkImage dstImage, VkImage srcImage, VkExtent3D extent, uint32_t dstLayer);
        void copyBufferToImage(VkImage dstImage, VkBuffer srcBuffer, VkExtent3D extent, uint32_t dstLayer);

        std::pair<VkImage, MemoryChunk> createImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps);

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        VulkanContext* m_context;

        VkDevice m_device;
        VkQueue m_graphicsQueue; // Implicitly cleaned up with VkDevice
        VkQueue m_presentQueue;

        VkCommandPool m_commandPool;

        std::map<uint32_t, MemoryHeap> m_memoryHeaps;

        std::map<VkBuffer, MemoryChunk> m_deviceBuffers;
        std::map<VkBuffer, MemoryChunk> m_stagingBuffers;

        std::map<VkImage, MemoryChunk> m_deviceImages;
    };
}