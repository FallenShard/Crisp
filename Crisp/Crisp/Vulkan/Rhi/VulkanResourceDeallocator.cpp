#include <Crisp/Vulkan/Rhi/VulkanResourceDeallocator.hpp>

#include <algorithm>

#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("VulkanResourceDeallocator");
} // namespace

VulkanResourceDeallocator::VulkanResourceDeallocator(
    const VkDevice device, const VmaAllocator allocator, const uint32_t framesInFlight)
    : m_deviceHandle(device)
    , m_allocator(allocator)
    , m_framesInFlight(framesInFlight) {}

VulkanResourceDeallocator::~VulkanResourceDeallocator() {
    freeAllResources();
}

void VulkanResourceDeallocator::setRetirementValue(const uint64_t value) {
    m_retirementValue = value;
}

uint64_t VulkanResourceDeallocator::getRetirementValue() const {
    return m_retirementValue;
}

void VulkanResourceDeallocator::collect(const uint64_t completedValue) {
    const auto isRetired = [completedValue](const auto& entry) { return entry.retirementValue <= completedValue; };

    // Destroy the handles before releasing their backing allocations.
    for (const auto& destructor : m_deferredDestructors) {
        if (isRetired(destructor)) {
            destructor.destructorCallback(destructor.handle, this);
        }
    }
    auto [firstDest, lastDest] = std::ranges::remove_if(m_deferredDestructors, isRetired);
    m_deferredDestructors.erase(firstDest, lastDest);

    for (const auto& allocation : m_deferredMemoryDeallocations) {
        if (isRetired(allocation)) {
            vmaFreeMemory(m_allocator, allocation.allocation);
        }
    }
    auto [firstVmaAlloc, lastVmaAlloc] = std::ranges::remove_if(m_deferredMemoryDeallocations, isRetired);
    m_deferredMemoryDeallocations.erase(firstVmaAlloc, lastVmaAlloc);
}

void VulkanResourceDeallocator::freeAllResources() {
    for (const auto& destructor : m_deferredDestructors) {
        destructor.destructorCallback(destructor.handle, this);
    }
    m_deferredDestructors.clear();

    for (const auto& allocation : m_deferredMemoryDeallocations) {
        vmaFreeMemory(m_allocator, allocation.allocation);
    }
    m_deferredMemoryDeallocations.clear();
}
} // namespace crisp
