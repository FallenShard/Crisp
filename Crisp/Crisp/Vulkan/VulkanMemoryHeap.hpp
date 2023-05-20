#pragma once

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Result.hpp>

#include <list>
#include <map>
#include <string>

namespace crisp
{
class VulkanMemoryHeap
{
public:
    struct AllocationBlock
    {
        VulkanMemoryHeap* heap;
        VkDeviceMemory memory;
        void* mappedPtr;
        std::map<uint64_t, uint64_t> freeChunks;
        robin_hood::unordered_flat_map<uint64_t, uint64_t> usedChunks;

        inline bool operator==(const AllocationBlock& other) const
        {
            return memory == other.memory;
        }

        void free(uint64_t offset, uint64_t size);
        void coalesce();
        std::pair<uint64_t, uint64_t> findFreeChunk(uint64_t size, uint64_t alignment) const;
    };

    struct Allocation
    {
        AllocationBlock* allocationBlock;
        uint64_t offset;
        uint64_t size;

        inline void free() const
        {
            allocationBlock->heap->free(*this);
        }

        inline void* getBlockMappedPtr() const
        {
            return allocationBlock->mappedPtr;
        }

        inline char* getMappedPtr() const
        {
            return static_cast<char*>(allocationBlock->mappedPtr) + offset;
        }

        inline VkDeviceMemory getMemory() const
        {
            return allocationBlock->memory;
        }
    };

    VulkanMemoryHeap(
        VkMemoryPropertyFlags memProps,
        VkDeviceSize blockSize,
        uint32_t memoryTypeIndex,
        VkDevice device,
        std::string tag);
    ~VulkanMemoryHeap();

    Result<Allocation> allocate(uint64_t size, uint64_t alignment);
    void free(const Allocation& allocation);

    bool isFromHeapIndex(uint32_t heapIndex, VkMemoryPropertyFlags memoryProperties) const;

    inline VkDeviceSize getUsedSize() const
    {
        return m_usedSize;
    }

    inline VkDeviceSize getAllocatedSize() const
    {
        return m_allocationBlocks.size() * m_blockSize;
    }

private:
    AllocationBlock allocateBlock(uint64_t size);
    void freeBlock(const AllocationBlock& allocationBlock);

    VkDevice m_device;
    VkMemoryPropertyFlags m_properties;
    uint32_t m_memoryTypeIndex;

    VkDeviceSize m_usedSize;
    VkDeviceSize m_blockSize;
    std::list<AllocationBlock> m_allocationBlocks;

    std::string m_tag;
};
} // namespace crisp
