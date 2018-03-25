#include "VulkanMemoryHeap.hpp"

#include <iostream>

#include <CrispCore/ConsoleUtils.hpp>

namespace crisp
{
    VulkanMemoryHeap::VulkanMemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize blockSize, uint32_t memTypeIdx, VkDevice device, std::string tag)
        : device(device)
        , properties(memProps)
        , totalSize(blockSize)
        , blockSize(blockSize)
        , usedSize(0)
        , memoryTypeIndex(memTypeIdx)
        , tag(tag)
    {
        allocateVulkanMemoryBlock(blockSize);
    }

    void VulkanMemoryHeap::free(uint64_t offset, uint64_t size, uint32_t blockIndex)
    {
        usedSize -= size;
        memoryBlocks[blockIndex].freeChunks[offset] = size;

        if (memoryBlocks[blockIndex].freeChunks.size() > 5)
            coalesce(blockIndex);

        if (memoryBlocks[blockIndex].freeChunks.size() > 10)
        {
            ConsoleColorizer colorizer(ConsoleColor::LightRed);
            //std::cout << "WARNING: Possible memory fragmentation in " << tag << '\n';
        }
    }

    void VulkanMemoryHeap::coalesce(uint32_t blockIndex)
    {
        auto& freeChunks = memoryBlocks[blockIndex].freeChunks;
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

    VulkanMemoryChunk VulkanMemoryHeap::allocate(uint64_t size, uint64_t alignment)
    {
        VulkanMemoryChunk allocResult = { nullptr, -1, 0, 0 };
        uint32_t foundMemoryBlock = -1;
        uint64_t foundChunkOffset = 0;
        uint64_t foundChunkSize   = 0;
        for (uint32_t i = 0; i < memoryBlocks.size(); ++i)
        {
            std::tie(foundChunkOffset, foundChunkSize) = findFreeChunkInBlock(i, size, alignment);

            if (foundChunkSize != 0)
            {
                foundMemoryBlock = i;
                break;
            }
        }

        if (foundMemoryBlock == -1)
        {
            ConsoleColorizer colorizer(ConsoleColor::Yellow);
            std::cout << '[' << tag << ']' << " Failed to find a free chunk in " << std::endl;
            std::cout << '[' << tag << ']' << " Allocation another memory block of size " << blockSize << std::endl;
            uint32_t blockIndex = allocateVulkanMemoryBlock(blockSize);
            std::tie(foundChunkOffset, foundChunkSize) = findFreeChunkInBlock(blockIndex, size, alignment);
            if (foundChunkSize != 0)
            {
                foundMemoryBlock = blockIndex;
                totalSize += blockSize;
            }
            else
            {
                ConsoleColorizer colorizer(ConsoleColor::LightRed);
                std::cout << '[' << tag << ']' << " CRITICAL ERROR: Allocation failed! " << blockSize << '\n';
                std::cout << '[' << tag << ']' << " Requested size: " << size << " bytes, but heap supports max " << blockSize << " byte allocations.\n";
                return allocResult;
            }
        }

        uint64_t alignedOffset = foundChunkOffset + ((alignment - (foundChunkOffset & (alignment - 1))) & (alignment - 1));
        uint64_t usableSize    = foundChunkSize - (alignedOffset - foundChunkSize);

        allocResult = { this, foundMemoryBlock, alignedOffset, size };

        usedSize += allocResult.size;

        auto& freeChunks = memoryBlocks[foundMemoryBlock].freeChunks;

        // Erase the modified chunk
        freeChunks.erase(foundChunkOffset);

        // Possibly add the left chunk
        if (allocResult.offset > foundChunkOffset)
            freeChunks[foundChunkOffset] = allocResult.offset - foundChunkOffset;

        // Possibly add the right chunk
        if (foundChunkOffset + foundChunkSize > allocResult.offset + allocResult.size)
            freeChunks[allocResult.offset + allocResult.size] = foundChunkOffset + foundChunkSize - (allocResult.offset + allocResult.size);

        return allocResult;
    }

    uint32_t VulkanMemoryHeap::allocateVulkanMemoryBlock(uint64_t size)
    {
        VkMemoryAllocateInfo devAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        devAllocInfo.allocationSize  = blockSize;
        devAllocInfo.memoryTypeIndex = memoryTypeIndex;

        VkDeviceMemory memory;
        vkAllocateMemory(device, &devAllocInfo, nullptr, &memory);

        uint32_t numBlocks = memoryBlocks.size();
        memoryBlocks.resize(numBlocks + 1);
        memoryBlocks[numBlocks].memory = memory;
        memoryBlocks[numBlocks].freeChunks.insert(std::make_pair(0, size));
        return numBlocks;
    }

    std::pair<uint64_t, uint64_t> VulkanMemoryHeap::findFreeChunkInBlock(uint32_t blockIndex, uint64_t size, uint64_t alignment)
    {
        uint64_t foundChunkOffset = 0;
        uint64_t foundChunkSize   = 0;

        for (auto& freeChunk : memoryBlocks[blockIndex].freeChunks)
        {
            uint64_t chunkOffset = freeChunk.first;
            uint64_t chunkSize   = freeChunk.second;

            uint64_t alignedOffset = chunkOffset + ((alignment - (chunkOffset & (alignment - 1))) & (alignment - 1));
            uint64_t usableSize    = chunkSize - (alignedOffset - chunkOffset);

            if (size <= usableSize)
            {
                return { chunkOffset, chunkSize };
            }
        }
        return { 0, 0 };
    }

    void VulkanMemoryHeap::printFreeChunks()
    {
        std::cout << tag << "\n";
        for (int i = 0; i < memoryBlocks.size(); i++)
        {
            std::cout << "Memory Block " << i << ":\n";
            for (auto& chunk : memoryBlocks[i].freeChunks)
                std::cout << "    " << chunk.first << " - " << chunk.second << "\n";
        }
    }

    void VulkanMemoryHeap::freeVulkanMemoryBlocks()
    {
        for (auto& block : memoryBlocks)
            vkFreeMemory(device, block.memory, nullptr);
    }

    void* VulkanMemoryHeap::mapMemoryBlock(uint32_t blockIndex) const
    {
        if (!(properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        {
            ConsoleColorizer colorizer(ConsoleColor::LightRed);
            std::cout << '[' << tag << ']' << " Attempted invalid MapMemory!\n";
            return nullptr;
        }

        void* mappedPtr;
        vkMapMemory(device, memoryBlocks[blockIndex].memory, 0, blockSize, 0, &mappedPtr);
        return mappedPtr;
    }

    void VulkanMemoryHeap::unmapMemoryBlock(uint32_t blockIndex) const
    {
        vkUnmapMemory(device, memoryBlocks[blockIndex].memory);
    }
}