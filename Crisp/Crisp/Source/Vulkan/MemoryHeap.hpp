#pragma once

#include <map>
#include <string>

#include <vulkan/vulkan.h>

namespace crisp
{
    struct MemoryHeap;
    struct MemoryChunk
    {
        MemoryHeap* memoryHeap;
        uint64_t offset;
        uint64_t size;
    };

    struct MemoryHeap
    {
        VkDeviceMemory memory;
        VkMemoryPropertyFlags properties;
        VkDeviceSize size;
        std::map<uint64_t, uint64_t> freeChunks;
        int32_t memoryTypeIndex;
        VkDeviceSize usedSize;
        std::string tag;

        MemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize size, uint32_t memoryTypeIndex, VkDevice device, std::string tag);

        void coalesce();
        void free(uint64_t offset, uint64_t size);
        MemoryChunk allocateChunk(uint64_t size, uint64_t alignment);
    };
}
