#pragma once

#include <map>
#include <string>
#include <list>

#include <vulkan/vulkan.h>

namespace crisp
{
    namespace internal
    {
        struct VulkanAllocationBlock
        {
            VkDeviceMemory memory;
            void* mappedPtr;
            std::map<uint64_t, uint64_t> freeChunks;

            inline bool operator==(const VulkanAllocationBlock& other) const { return memory == other.memory; }
        };
    }

    struct VulkanMemoryHeap;
    struct VulkanMemoryChunk
    {
        VulkanMemoryHeap* memoryHeap;
        internal::VulkanAllocationBlock* allocationBlock;
        uint64_t offset;
        uint64_t size;

        inline void free();
        inline VkDeviceMemory getMemory() const;
        inline void* getBlockMappedPtr() const;
        inline char* getMappedPtr() const;
    };

    struct VulkanMemoryHeap
    {
        VkDevice device;
        VkMemoryPropertyFlags properties;
        uint32_t              memoryTypeIndex;

        VkDeviceSize usedSize;
        VkDeviceSize blockSize;
        std::list<internal::VulkanAllocationBlock> memoryBlocks;

        std::string tag;

        VulkanMemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize blockSize, uint32_t memoryTypeIndex, VkDevice device, std::string tag);

        void coalesce(internal::VulkanAllocationBlock& block);
        void free(uint64_t offset, uint64_t size, internal::VulkanAllocationBlock& block);
        VulkanMemoryChunk allocate(uint64_t size, uint64_t alignment);
        internal::VulkanAllocationBlock* allocateVulkanMemoryBlock(uint64_t size);
        std::pair<uint64_t, uint64_t> findFreeChunkInBlock(internal::VulkanAllocationBlock& block, uint64_t size, uint64_t alignment);
        void printFreeChunks();

        void freeVulkanMemoryBlocks();

        void* mapMemoryBlock(internal::VulkanAllocationBlock& blockIndex) const;

        inline VkDeviceSize getTotalAllocatedSize() const { return memoryBlocks.size() * blockSize; }
    };

    inline void VulkanMemoryChunk::free()
    {
        memoryHeap->free(offset, size, *allocationBlock);
    }

    inline VkDeviceMemory VulkanMemoryChunk::getMemory() const
    {
        return allocationBlock->memory;
    }

    inline void* VulkanMemoryChunk::getBlockMappedPtr() const
    {
        return allocationBlock->mappedPtr;
    }

    inline char* VulkanMemoryChunk::getMappedPtr() const
    {
        return static_cast<char*>(allocationBlock->mappedPtr) + offset;
    }
}
