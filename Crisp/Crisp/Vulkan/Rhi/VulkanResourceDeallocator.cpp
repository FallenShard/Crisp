#include <Crisp/Vulkan/Rhi/VulkanResourceDeallocator.hpp>

#include <algorithm>

#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("VulkanResourceDeallocator");
} // namespace

VulkanResourceDeallocator::VulkanResourceDeallocator(VkDevice device, VmaAllocator allocator)
    : m_deviceHandle(device)
    , m_allocator(allocator) {}

VulkanResourceDeallocator::~VulkanResourceDeallocator() {
    freeAllResources();
}

void VulkanResourceDeallocator::advanceFrame() {
    m_frameIndex++;

    // Handle deferred destructors
    for (const auto& destructor : m_deferredDestructors) {
        if (destructor.frameIndex < m_frameIndex) {
            destructor.destructorCallback(destructor.handle, this);
        }
    }
    auto [firstDest, lastDest] = std::ranges::remove_if(m_deferredDestructors, [this](const auto& a) {
        return a.frameIndex < m_frameIndex;
    });
    m_deferredDestructors.erase(firstDest, lastDest);

    // Free the memory chunks
    for (const auto& allocation : m_deferredMemoryDeallocations) {
        if (allocation.frameIndex < m_frameIndex) {
            vmaFreeMemory(m_allocator, allocation.allocation);
        }
    }
    auto [firstVmaAlloc, lastVmaAlloc] = std::ranges::remove_if(m_deferredMemoryDeallocations, [this](const auto& a) {
        return a.frameIndex < m_frameIndex;
    });
    m_deferredMemoryDeallocations.erase(firstVmaAlloc, lastVmaAlloc);
}

void VulkanResourceDeallocator::freeAllResources() {
    for (auto& destructor : m_deferredDestructors) {
        destructor.destructorCallback(destructor.handle, this);
    }
    m_deferredDestructors.clear();

    for (const auto& allocation : m_deferredMemoryDeallocations) {
        vmaFreeMemory(m_allocator, allocation.allocation);
    }
    m_deferredMemoryDeallocations.clear();
}
} // namespace crisp
