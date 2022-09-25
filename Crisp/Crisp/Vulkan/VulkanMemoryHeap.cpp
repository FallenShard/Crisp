#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>

#include <Crisp/Common/Logger.hpp>

namespace crisp
{
VulkanMemoryHeap::VulkanMemoryHeap(
    VkMemoryPropertyFlags memProps, VkDeviceSize blockSize, uint32_t memTypeIdx, VkDevice device, std::string tag)
    : m_device(device)
    , m_properties(memProps)
    , m_memoryTypeIndex(memTypeIdx)
    , m_usedSize(0)
    , m_blockSize(blockSize)
    , m_tag(tag)
{
}

VulkanMemoryHeap::~VulkanMemoryHeap()
{
    for (const auto& block : m_allocationBlocks)
        freeBlock(block);
}

Result<VulkanMemoryHeap::Allocation> VulkanMemoryHeap::allocate(uint64_t size, uint64_t alignment)
{
    if (size > m_blockSize)
    {
        return resultError(
            "Requested allocation size is greater than the block size: {} MB > {} MB!", size >> 20, m_blockSize >> 20);
    }

    Allocation allocation{nullptr, 0, 0};
    AllocationBlock* allocationBlock = nullptr;
    uint64_t foundChunkOffset = 0;
    uint64_t foundChunkSize = 0;
    for (auto& block : m_allocationBlocks)
    {
        std::tie(foundChunkOffset, foundChunkSize) = block.findFreeChunk(size, alignment);
        if (foundChunkSize != 0)
        {
            allocationBlock = &block;
            break;
        }
    }

    if (!allocationBlock)
    {
        spdlog::info("[{}] Allocating a block of size: {} MB.", m_tag, (m_blockSize >> 20));
        m_allocationBlocks.push_back(allocateBlock(m_blockSize));
        std::tie(foundChunkOffset, foundChunkSize) = m_allocationBlocks.back().findFreeChunk(size, alignment);
        if (foundChunkSize == 0)
        {
            return resultError(
                "Failed to find the requested size in a newly allocated block! {} MB > {} MB.",
                size >> 20,
                m_blockSize >> 20);
        }

        allocationBlock = &m_allocationBlocks.back();
    }

    const uint64_t alignedOffset =
        foundChunkOffset + ((alignment - (foundChunkOffset & (alignment - 1))) & (alignment - 1));

    allocation = {allocationBlock, alignedOffset, size};

    m_usedSize += allocation.size;

    auto& freeChunks = allocationBlock->freeChunks;

    // Erase the modified chunk
    freeChunks.erase(foundChunkOffset);

    // Possibly add the left chunk
    if (allocation.offset > foundChunkOffset)
        freeChunks[foundChunkOffset] = allocation.offset - foundChunkOffset;

    // Possibly add the right chunk
    if (foundChunkOffset + foundChunkSize > allocation.offset + allocation.size)
        freeChunks[allocation.offset + allocation.size] =
            foundChunkOffset + foundChunkSize - (allocation.offset + allocation.size);

    allocationBlock->usedChunks[alignedOffset] = size;
    return allocation;
}

void VulkanMemoryHeap::free(const Allocation& allocation)
{
    AllocationBlock& block = *allocation.allocationBlock;
    spdlog::info("[{}] Freeing a suballocation [{}, {}]", m_tag, allocation.offset, allocation.size);
    m_usedSize -= allocation.size;
    block.freeChunks[allocation.offset] = allocation.size;
    block.usedChunks.erase(allocation.offset);

    if (block.freeChunks.size() > 1)
    {
        block.coalesce();
    }

    if (block.freeChunks.size() > 10)
    {
        spdlog::warn("[{}] Possible memory fragmentation - free chunks: {}", m_tag, block.freeChunks.size());
    }

    if (block.freeChunks.size() == 1 && block.freeChunks.begin()->second == m_blockSize)
    {
        freeBlock(block);
        m_allocationBlocks.remove(block);

        spdlog::info("[{}] Freed a memory block.", m_tag);
    }
}

bool VulkanMemoryHeap::isFromHeapIndex(uint32_t heapIndex, VkMemoryPropertyFlags memoryProperties) const
{
    return m_memoryTypeIndex == heapIndex && m_properties == memoryProperties;
}

VulkanMemoryHeap::AllocationBlock VulkanMemoryHeap::allocateBlock(uint64_t size)
{
    VkMemoryAllocateFlagsInfo allocateFlagsInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    allocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo devAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    devAllocInfo.pNext = &allocateFlagsInfo;
    devAllocInfo.allocationSize = size;
    devAllocInfo.memoryTypeIndex = m_memoryTypeIndex;

    VkDeviceMemory memory;
    vkAllocateMemory(m_device, &devAllocInfo, nullptr, &memory);

    AllocationBlock block{};
    block.heap = this;
    block.memory = memory;
    block.freeChunks.emplace(0, size);
    if (m_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        vkMapMemory(m_device, memory, 0, size, 0, &block.mappedPtr);
    }
    return block;
}

void VulkanMemoryHeap::freeBlock(const AllocationBlock& allocationBlock)
{
    if (allocationBlock.mappedPtr)
    {
        vkUnmapMemory(m_device, allocationBlock.memory);
    }
    vkFreeMemory(m_device, allocationBlock.memory, nullptr);
}

void VulkanMemoryHeap::AllocationBlock::coalesce()
{
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

std::pair<uint64_t, uint64_t> VulkanMemoryHeap::AllocationBlock::findFreeChunk(uint64_t size, uint64_t alignment) const
{
    for (const auto& freeChunk : freeChunks)
    {
        const uint64_t chunkOffset = freeChunk.first;
        const uint64_t chunkSize = freeChunk.second;

        const uint64_t alignedOffset = chunkOffset + ((alignment - (chunkOffset & (alignment - 1))) & (alignment - 1));
        const uint64_t usableSize = chunkSize - (alignedOffset - chunkOffset);

        if (size <= usableSize)
        {
            return {chunkOffset, chunkSize};
        }
    }
    return {0, 0};
}
} // namespace crisp