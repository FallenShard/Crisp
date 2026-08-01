#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
class VulkanResourceDeallocator;
using VulkanDestructorCallback = void (*)(void*, VulkanResourceDeallocator*);

struct DeferredHandleDestructor {
    uint64_t retirementValue;
    void* handle;
    VulkanDestructorCallback destructorCallback;
};

struct DeferredMemoryDestructor {
    uint64_t retirementValue;
    VmaAllocation allocation;
};

class VulkanResourceDeallocator {
public:
    VulkanResourceDeallocator(VkDevice device, VmaAllocator allocator, uint32_t framesInFlight);
    ~VulkanResourceDeallocator();

    VulkanResourceDeallocator(const VulkanResourceDeallocator&) = delete;
    VulkanResourceDeallocator& operator=(const VulkanResourceDeallocator&) = delete;

    VulkanResourceDeallocator(VulkanResourceDeallocator&&) noexcept = default;
    VulkanResourceDeallocator& operator=(VulkanResourceDeallocator&&) noexcept = default;

    void setRetirementValue(uint64_t value);
    uint64_t getRetirementValue() const;

    void collect(uint64_t completedValue);

    void freeAllResources();

    template <typename VulkanHandleType>
    void deferDestruction(const VulkanHandleType handle, const VulkanDestructorCallback callback) {
        m_deferredDestructors.push_back({m_retirementValue, handle, callback});
    }

    template <typename VulkanHandleType>
    void deferDestruction(const VulkanHandleType handle) {
        deferDestructionAt(m_retirementValue, handle);
    }

    // Escape hatch for work submitted outside the owner's regular cadence, which knows its own protecting value.
    template <typename VulkanHandleType>
    void deferDestructionAt(const uint64_t retirementValue, const VulkanHandleType handle) {
        m_deferredDestructors.push_back(
            {retirementValue, handle, [](void* handle, VulkanResourceDeallocator* deallocator) {
                 getDestroyFunc<VulkanHandleType>()(
                     deallocator->getDeviceHandle(), static_cast<VulkanHandleType>(handle), nullptr);
             }});
    }

    void deferMemoryDeallocation(const VmaAllocation allocation) {
        deferMemoryDeallocationAt(m_retirementValue, allocation);
    }

    void deferMemoryDeallocationAt(const uint64_t retirementValue, const VmaAllocation allocation) {
        m_deferredMemoryDeallocations.emplace_back(retirementValue, allocation);
    }

    VkDevice getDeviceHandle() const {
        return m_deviceHandle;
    }

    VmaAllocator getMemoryAllocator() const {
        return m_allocator;
    }

    size_t getDeferredDestructorCount() const {
        return m_deferredDestructors.size();
    }

    size_t getDeferredMemoryDeallocationCount() const {
        return m_deferredMemoryDeallocations.size();
    }

    uint32_t getFramesInFlight() const {
        return m_framesInFlight;
    }

private:
    VkDevice m_deviceHandle{VK_NULL_HANDLE};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    uint32_t m_framesInFlight{0};
    uint64_t m_retirementValue{0};

    std::vector<DeferredHandleDestructor> m_deferredDestructors;
    std::vector<DeferredMemoryDestructor> m_deferredMemoryDeallocations;
};
} // namespace crisp
