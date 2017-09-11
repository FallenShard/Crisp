#pragma once

#include <map>
#include <string>

#include <vulkan/vulkan.h>

namespace crisp
{
    struct VulkanMemoryHeap;
    struct VulkanMemoryChunk
    {
        VulkanMemoryHeap* memoryHeap;
        uint64_t offset;
        uint64_t size;

        inline void free();
    };

    struct VulkanMemoryHeap
    {
        VkDeviceMemory        memory;
        VkMemoryPropertyFlags properties;
        uint32_t              memoryTypeIndex;

        VkDeviceSize                 size;
        VkDeviceSize                 usedSize;
        std::map<uint64_t, uint64_t> freeChunks;
        
        std::string tag;

        VulkanMemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize size, uint32_t memoryTypeIndex, VkDevice device, std::string tag);

        void coalesce();
        inline void free(const VulkanMemoryChunk& chunk) { free(chunk.offset, chunk.size); }
        void free(uint64_t offset, uint64_t size);
        VulkanMemoryChunk allocateChunk(uint64_t size, uint64_t alignment);
    };

    inline void VulkanMemoryChunk::free()
    {
        memoryHeap->free(offset, size);
    }
}
