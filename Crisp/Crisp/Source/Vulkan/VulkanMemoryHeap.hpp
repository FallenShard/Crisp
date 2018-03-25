#pragma once

#include <map>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    struct VulkanMemoryHeap;
    struct VulkanMemoryChunk
    {
        VulkanMemoryHeap* memoryHeap;
        uint32_t blockIndex;
        uint64_t offset;
        uint64_t size;

        inline void free();
        inline VkDeviceMemory getMemory() const;
    };

    namespace internal
    {
        struct VulkanAllocationBlock
        {
            VkDeviceMemory memory;
            std::map<uint64_t, uint64_t> freeChunks;
        };
    }

    struct VulkanMemoryHeap
    {
        VkDevice device;
        VkMemoryPropertyFlags properties;
        uint32_t              memoryTypeIndex;

        VkDeviceSize totalSize;
        VkDeviceSize usedSize;
        VkDeviceSize blockSize;
        std::vector<internal::VulkanAllocationBlock> memoryBlocks;

        std::string tag;

        VulkanMemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize blockSize, uint32_t memoryTypeIndex, VkDevice device, std::string tag);

        void coalesce(uint32_t blockIndex);
        inline void free(const VulkanMemoryChunk& chunk) { free(chunk.offset, chunk.size, chunk.blockIndex); }
        void free(uint64_t offset, uint64_t size, uint32_t blockIndex);
        VulkanMemoryChunk allocate(uint64_t size, uint64_t alignment);
        uint32_t allocateVulkanMemoryBlock(uint64_t size);
        std::pair<uint64_t, uint64_t> findFreeChunkInBlock(uint32_t blockIndex, uint64_t size, uint64_t alignment);
        void printFreeChunks();

        void freeVulkanMemoryBlocks();

        void* mapMemoryBlock(uint32_t blockIndex) const;
        void unmapMemoryBlock(uint32_t blockIndex) const;
    };

    inline void VulkanMemoryChunk::free()
    {
        memoryHeap->free(offset, size, blockIndex);
    }

    inline VkDeviceMemory VulkanMemoryChunk::getMemory() const
    {
        return memoryHeap->memoryBlocks[blockIndex].memory;
    }
}
