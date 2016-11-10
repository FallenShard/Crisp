#pragma once

#include <map>
#include <set>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanContext;

    struct MemoryHeap
    {
        VkDeviceMemory memory;
        VkMemoryPropertyFlags properties;
        VkDeviceSize size;
        std::map<uint64_t, uint64_t> freeChunks;
        int32_t memoryTypeIndex;

        MemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize size, uint32_t memoryTypeIndex, VkDevice device);

        void coalesce();
        void free(uint64_t offset, uint64_t size);
        std::pair<uint64_t, uint64_t> allocateChunk(uint64_t size, uint64_t alignment);
    };

    struct AllocationDesc
    {
        MemoryHeap* memoryHeap;
        uint64_t offset;
        uint64_t size;
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

        void fillDeviceBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size);
        VkBuffer createDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps);
        VkBuffer createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps);
        void updateDeviceBuffer(VkBuffer dstBuffer, VkBuffer stagingBuffer, const void* srcData, VkDeviceSize size);

        VkImage createDeviceImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps);
        void fillDeviceImage(VkImage dstImage, const void* srcData, VkDeviceSize size, uint32_t width, uint32_t height);
        void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const;

        void freeResources();
        void destroyDeviceImage(VkImage image);

        void printMemoryStatus();

    private:
        std::pair<VkBuffer, AllocationDesc> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
        void copyBuffer(VkBuffer dstBuffer, VkBuffer srcBuffer, VkDeviceSize size);

        std::pair<VkImage, AllocationDesc> createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps);
        void copyImage(VkImage dstImage, VkImage srcImage, uint32_t width, uint32_t height);

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        VulkanContext* m_context;

        VkDevice m_device;
        VkQueue m_graphicsQueue; // Implicitly cleaned up with VkDevice
        VkQueue m_presentQueue;

        VkCommandPool m_commandPool;

        std::map<uint32_t, MemoryHeap> m_memoryHeaps;

        std::map<VkBuffer, AllocationDesc> m_deviceBuffers;
        std::map<VkBuffer, AllocationDesc> m_stagingBuffers;

        std::map<VkImage, AllocationDesc> m_deviceImages;
    };
}