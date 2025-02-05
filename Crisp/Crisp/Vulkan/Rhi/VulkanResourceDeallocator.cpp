#include <Crisp/Vulkan/Rhi/VulkanResourceDeallocator.hpp>

#include <algorithm>

namespace crisp {
VulkanResourceDeallocator::VulkanResourceDeallocator(VkDevice device, VmaAllocator allocator, int32_t virtualFrameCount)
    : m_deviceHandle(device)
    , m_allocator(allocator)
    , m_virtualFrameCount{virtualFrameCount} {}

VulkanResourceDeallocator::~VulkanResourceDeallocator() {
    freeAllResources();
}

void VulkanResourceDeallocator::decrementLifetimes() {
    // Handle deferred destructors
    for (auto& destructor : m_deferredDestructors) {
        if (--destructor.framesRemaining < 0) {
            destructor.destructorCallback(destructor.vulkanHandle, this);
        }
    }

    auto [firstDest, lastDest] = std::ranges::remove_if(m_deferredDestructors, [](const auto& a) {
        return a.framesRemaining < 0;
    });

    m_deferredDestructors.erase(firstDest, lastDest);

    // Free the memory chunks
    for (auto& memoryChunkPair : m_deferredVmaDeallocations) {
        if (--memoryChunkPair.first < 0) {
            vmaFreeMemory(m_allocator, memoryChunkPair.second);
        }
    }

    auto [firstVmaAlloc, lastVmaAlloc] = std::ranges::remove_if(m_deferredVmaDeallocations, [](const auto& a) {
        return a.first < 0;
    });

    m_deferredVmaDeallocations.erase(firstVmaAlloc, lastVmaAlloc);
}

void VulkanResourceDeallocator::freeAllResources() {
    for (auto& destructor : m_deferredDestructors) {
        destructor.destructorCallback(destructor.vulkanHandle, this);
    }
    m_deferredDestructors.clear();

    for (auto& memoryChunkPair : m_deferredVmaDeallocations) {
        vmaFreeMemory(m_allocator, memoryChunkPair.second);
    }
    m_deferredVmaDeallocations.clear();
}
} // namespace crisp
