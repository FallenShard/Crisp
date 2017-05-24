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

        inline void free();
    };

    struct MemoryHeap
    {
        VkDeviceMemory        memory;
        VkMemoryPropertyFlags properties;
        uint32_t              memoryTypeIndex;

        VkDeviceSize                 size;
        VkDeviceSize                 usedSize;
        std::map<uint64_t, uint64_t> freeChunks;
        
        std::string tag;

        MemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize size, uint32_t memoryTypeIndex, VkDevice device, std::string tag);

        void coalesce();
        inline void free(const MemoryChunk& chunk) { free(chunk.offset, chunk.size); }
        void free(uint64_t offset, uint64_t size);
        MemoryChunk allocateChunk(uint64_t size, uint64_t alignment);
    };

    inline void MemoryChunk::free()
    {
        memoryHeap->free(offset, size);
    }
}
