#include "VulkanMemoryHeap.hpp"

#include <iostream>
#include <algorithm>

#include <CrispCore/ConsoleUtils.hpp>

namespace crisp
{
    VulkanMemoryHeap::VulkanMemoryHeap(VkMemoryPropertyFlags memProps, VkDeviceSize blockSize, uint32_t memTypeIdx, VkDevice device, std::string tag)
        : device(device)
        , properties(memProps)
        , blockSize(blockSize)
        , usedSize(0)
        , memoryTypeIndex(memTypeIdx)
        , tag(tag)
    {
        allocateVulkanMemoryBlock(blockSize);
    }

    void VulkanMemoryHeap::free(uint64_t offset, uint64_t size, internal::VulkanAllocationBlock& block)
    {
        usedSize -= size;
        block.freeChunks[offset] = size;

        if (block.freeChunks.size() > 1)
            coalesce(block);

        if (block.freeChunks.size() > 10)
        {
            ConsoleColorizer colorizer(ConsoleColor::Yellow);
            std::cout << '[' << tag << ']' << " Possible memory fragmentation - free chunks: " << block.freeChunks.size() << '\n';
        }
        else if (block.freeChunks.size() == 1 && block.freeChunks.begin()->second == blockSize)
        {
            if (block.mappedPtr)
                vkUnmapMemory(device, block.memory);

            vkFreeMemory(device, block.memory, nullptr);

            memoryBlocks.remove(block);

            ConsoleColorizer colorizer(ConsoleColor::Green);
            std::cout << '[' << tag << ']' << " Freeing a memory block\n";
        }
    }

    void VulkanMemoryHeap::coalesce(internal::VulkanAllocationBlock& block)
    {
        auto& freeChunks = block.freeChunks;
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
        VulkanMemoryChunk allocResult = { nullptr, nullptr, 0, 0 };
        internal::VulkanAllocationBlock* foundMemoryBlock = nullptr;
        uint64_t foundChunkOffset = 0;
        uint64_t foundChunkSize   = 0;
        for (auto& block : memoryBlocks)
        {
            std::tie(foundChunkOffset, foundChunkSize) = findFreeChunkInBlock(block, size, alignment);

            if (foundChunkSize != 0)
            {
                foundMemoryBlock = &block;
                break;
            }
        }

        if (!foundMemoryBlock)
        {
            ConsoleColorizer colorizer(ConsoleColor::Yellow);
            std::cout << '[' << tag << ']' << " Failed to find a free chunk.\n";
            std::cout << '[' << tag << ']' << " Allocating another memory block of size " << (blockSize >> 20) << " MB.\n";
            auto* blockPtr = allocateVulkanMemoryBlock(blockSize);
            std::tie(foundChunkOffset, foundChunkSize) = findFreeChunkInBlock(*blockPtr, size, alignment);
            if (foundChunkSize != 0)
            {
                foundMemoryBlock = blockPtr;
            }
            else
            {
                ConsoleColorizer colorizer(ConsoleColor::LightRed);
                std::cout << '[' << tag << ']' << " CRITICAL ERROR: Allocation failed! " << (blockSize >> 20) << " MB.\n";
                std::cout << '[' << tag << ']' << " Requested size: " << size << " bytes, but heap supports max " << (blockSize >> 20) << " MB allocations.\n";
                return allocResult;
            }
        }

        uint64_t alignedOffset = foundChunkOffset + ((alignment - (foundChunkOffset & (alignment - 1))) & (alignment - 1));

        allocResult = { this, foundMemoryBlock, alignedOffset, size };

        usedSize += allocResult.size;

        auto& freeChunks = foundMemoryBlock->freeChunks;

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

    internal::VulkanAllocationBlock* VulkanMemoryHeap::allocateVulkanMemoryBlock(uint64_t size)
    {
        VkMemoryAllocateInfo devAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        devAllocInfo.allocationSize  = blockSize;
        devAllocInfo.memoryTypeIndex = memoryTypeIndex;

        VkDeviceMemory memory;
        vkAllocateMemory(device, &devAllocInfo, nullptr, &memory);

        memoryBlocks.push_back(internal::VulkanAllocationBlock());
        auto& block = memoryBlocks.back();
        block.memory = memory;
        block.freeChunks.insert(std::make_pair(0, size));
        block.mappedPtr = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? mapMemoryBlock(block) : nullptr;
        return &block;
    }

    std::pair<uint64_t, uint64_t> VulkanMemoryHeap::findFreeChunkInBlock(internal::VulkanAllocationBlock& block, uint64_t size, uint64_t alignment)
    {
        uint64_t foundChunkOffset = 0;
        uint64_t foundChunkSize   = 0;

        for (auto& freeChunk : block.freeChunks)
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
        int i = 0;
        for (auto& block : memoryBlocks)
        {
            std::cout << "Memory Block " << i++ << ":\n";
            for (auto& chunk : block.freeChunks)
                std::cout << "    " << chunk.first << " - " << chunk.second << "\n";
        }
    }

    void VulkanMemoryHeap::freeVulkanMemoryBlocks()
    {
        for (auto& block : memoryBlocks)
        {
            if (block.mappedPtr)
                vkUnmapMemory(device, block.memory);

            vkFreeMemory(device, block.memory, nullptr);
        }
    }

    void* VulkanMemoryHeap::mapMemoryBlock(internal::VulkanAllocationBlock& block) const
    {
        if (!(properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        {
            ConsoleColorizer colorizer(ConsoleColor::LightRed);
            std::cout << '[' << tag << ']' << " Attempted invalid MapMemory!\n";
            return nullptr;
        }

        void* mappedPtr;
        vkMapMemory(device, block.memory, 0, blockSize, 0, &mappedPtr);
        return mappedPtr;
    }
}