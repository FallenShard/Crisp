#include "MemoryHeap.hpp"

namespace crisp
{
    MemoryHeap::MemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize allocSize, uint32_t memTypeIdx, VkDevice device)
        : memory(VK_NULL_HANDLE)
        , properties(memProps)
        , size(allocSize)
        , usedSize(0)
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
        usedSize -= size;
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

    MemoryChunk MemoryHeap::allocateChunk(uint64_t size, uint64_t alignment)
    {
        VkDeviceSize paddedSize = size + ((alignment - (size & (alignment - 1))) & (alignment - 1));

        MemoryChunk allocResult = { this, 0, 0 };
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

                allocResult.offset = paddedOffset;
                allocResult.size   = paddedSize;
                break;
            }
        }

        usedSize += allocResult.size;

        if (foundChunkSize > 0)
        {
            // Erase the modified chunk
            freeChunks.erase(foundChunkOffset);

            // Possibly add the left chunk
            if (allocResult.offset > foundChunkOffset)
                freeChunks[foundChunkOffset] = allocResult.offset - foundChunkOffset;

            // Possibly add the right chunk
            if (foundChunkOffset + foundChunkSize > allocResult.offset + allocResult.size)
                freeChunks[allocResult.offset + allocResult.size] = foundChunkOffset + foundChunkSize - (allocResult.offset + allocResult.size);
        }
        
        return allocResult;
    }
}