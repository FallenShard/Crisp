#include <Crisp/Vulkan/Rhi/VulkanResourceDeallocator.hpp>

#include <ranges>

namespace crisp {
VulkanResourceDeallocator::VulkanResourceDeallocator(VkDevice device, int32_t virtualFrameCount)
    : m_deviceHandle(device)
    , m_virtualFrameCount{virtualFrameCount} {
    m_handleTagMap.emplace(VK_NULL_HANDLE, "Unknown");
}

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
    for (auto& memoryChunkPair : m_deferredDeallocations) {
        if (--memoryChunkPair.first < 0) {
            memoryChunkPair.second.free();
        }
    }

    auto [firstAlloc, lastAlloc] = std::ranges::remove_if(m_deferredDeallocations, [](const auto& a) {
        return a.first < 0;
    });

    m_deferredDeallocations.erase(firstAlloc, lastAlloc);
}

void VulkanResourceDeallocator::freeAllResources() {
    for (auto& destructor : m_deferredDestructors) {
        destructor.destructorCallback(destructor.vulkanHandle, this);
    }
    m_deferredDestructors.clear();

    for (auto& memoryChunkPair : m_deferredDeallocations) {
        memoryChunkPair.second.free();
    }
    m_deferredDeallocations.clear();
}
} // namespace crisp
