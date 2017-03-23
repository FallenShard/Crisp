#include "MemoryHeap.hpp"

#include <iostream>

namespace crisp
{
    MemoryHeap::MemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize allocSize, uint32_t memTypeIdx, VkDevice device, std::string tag)
        : memory(VK_NULL_HANDLE)
        , properties(memProps)
        , size(allocSize)
        , usedSize(0)
        , memoryTypeIndex(memTypeIdx)
        , tag(tag)
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

        if (freeChunks.size() > 5)
            coalesce();

        if (freeChunks.size() > 10)
        {
            std::cout << "WARNING: Possible memory fragmentation in " << tag << std::endl;
        }
    }

    void MemoryHeap::coalesce()
    {
        std::map<uint64_t, uint64_t> newFreeMap;
        auto current = freeChunks.begin();
        auto next = std::next(freeChunks.begin());
        while (next != freeChunks.end())
        {
            auto currRight = current->first + current->second;
            if (currRight == next->first)
            {
                current->second = current->second + next->second;
                freeChunks.erase(next++);
            }
            else
            {
                current = next;
                next++;
            }
        }
    }

    MemoryChunk MemoryHeap::allocateChunk(uint64_t size, uint64_t alignment)
    {
        MemoryChunk allocResult = { this, 0, 0 };
        uint64_t foundChunkOffset = 0;
        uint64_t foundChunkSize = 0;
        for (auto& freeChunk : freeChunks)
        {
            uint64_t chunkOffset = freeChunk.first;
            uint64_t chunkSize = freeChunk.second;

            uint64_t paddedOffset = chunkOffset + ((alignment - (chunkOffset & (alignment - 1))) & (alignment - 1));
            uint64_t usableSize = chunkSize - (paddedOffset - chunkOffset);

            if (size <= usableSize)
            {
                foundChunkOffset = chunkOffset;
                foundChunkSize   = chunkSize;

                allocResult.offset = paddedOffset;
                allocResult.size   = size;
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

        if (allocResult.size == 0)
        {
            std::cout << "CRITICAL ERROR: Allocation failed in " << tag << std::endl;
            std::cout << "Requested size: " << size << std::endl;
        }
        
        return allocResult;
    }
}