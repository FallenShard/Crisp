#pragma once

#include <map>
#include <set>
#include <memory>

#include <vulkan/vulkan.h>
#include "MemoryHeap.hpp"

namespace crisp
{
    class VulkanContext;

    struct DeviceMemoryMetrics
    {
        uint64_t bufferMemorySize;
        uint64_t bufferMemoryUsed;
        uint64_t imageMemorySize;
        uint64_t imageMemoryUsed;
        uint64_t stagingMemorySize;
        uint64_t stagingMemoryUsed;
    };

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
        VkBuffer createDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* srcData = nullptr);
        VkBuffer createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usages);
        VkBuffer createStagingBuffer(VkBuffer srcBuffer, VkDeviceSize srcSize, VkDeviceSize newSize, VkBufferUsageFlags usage);
        void updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize size);
        void updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize stagingOffset, VkDeviceSize dstOffset, VkDeviceSize size);
        void updateStagingBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize offset, VkDeviceSize size);
        VkBuffer resizeStagingBuffer(VkBuffer prevBuffer, VkDeviceSize prevSize, VkDeviceSize newSize, VkBufferUsageFlags usage);
        void fillStagingBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size);
        void flushMappedRanges();

        VkImage createDeviceImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage);
        void fillDeviceImage(VkImage dstImage, const void* srcData, VkDeviceSize size, VkExtent3D extent, uint32_t numLayers, uint32_t firstLayer = 0);
        void updateDeviceImage(VkImage dstImage, VkBuffer stagingBuffer, VkDeviceSize size, VkExtent3D extent, uint32_t numLayers);
        void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t numLayers, uint32_t baseLayer = 0);

        VkImage createDeviceImageArray(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout = VK_IMAGE_LAYOUT_PREINITIALIZED, VkImageCreateFlags createFlags = 0);

        VkImageView createImageView(VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect, uint32_t baseLayer, uint32_t numLayers) const;

        VkSampler createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode);

        VkSemaphore createSemaphore();

        void freeResources();
        void destroyDeviceImage(VkImage image);
        void destroyDeviceBuffer(VkBuffer buffer);
        void destroyStagingBuffer(VkBuffer buffer);

        void printMemoryStatus();
        DeviceMemoryMetrics getDeviceMemoryUsage();

        std::pair<VkBuffer, MemoryChunk> createBuffer(MemoryHeap* heap, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);

        MemoryHeap* getDeviceBufferHeap() const;
        MemoryHeap* getStagingBufferHeap() const;
        void* getStagingMemoryPtr() const;

    private:
        static constexpr VkDeviceSize DeviceHeapSize  = 256 << 20; // 256 MB
        static constexpr VkDeviceSize StagingHeapSize = 128 << 20; // 128 MB

        void copyBuffer(VkBuffer dstBuffer, VkBuffer srcBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);

        void copyImage(VkImage dstImage, VkImage srcImage, VkExtent3D extent, uint32_t dstLayer);
        void copyBufferToImage(VkImage dstImage, VkBuffer srcBuffer, VkExtent3D extent, uint32_t dstLayer);

        std::pair<VkImage, MemoryChunk> createImage(MemoryHeap* heap, VkExtent3D extent, uint32_t layers, VkFormat format, VkImageLayout initLayout, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps, VkImageCreateFlags createFlags = 0);

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        VulkanContext* m_context;

        VkDevice m_device;
        VkQueue m_graphicsQueue; // Implicitly cleaned up with VkDevice
        VkQueue m_presentQueue;

        VkCommandPool m_commandPool;

        std::unique_ptr<MemoryHeap> m_deviceBufferHeap;
        std::unique_ptr<MemoryHeap> m_deviceImageHeap;
        std::unique_ptr<MemoryHeap> m_stagingBufferHeap;
        void* m_mappedStagingPtr;

        std::map<VkBuffer, MemoryChunk> m_deviceBuffers;
        std::map<VkBuffer, MemoryChunk> m_stagingBuffers;

        std::map<VkImage, MemoryChunk> m_deviceImages;
    };
}