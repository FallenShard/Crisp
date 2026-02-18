#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
class VulkanResourceDeallocator;
using VulkanDestructorCallback = void (*)(void*, VulkanResourceDeallocator*);

struct DeferredHandleDestructor {
    uint64_t frameIndex;
    void* handle;
    VulkanDestructorCallback destructorCallback;
};

struct DeferredMemoryDestructor {
    uint64_t frameIndex;
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

    void advanceFrame();

    void freeAllResources();

    template <typename VulkanHandleType>
    void deferDestruction(const VulkanHandleType handle, const VulkanDestructorCallback callback) {
        m_deferredDestructors.push_back({m_frameIndex + m_framesInFlight, handle, callback});
    }

    template <typename VulkanHandleType>
    void deferDestruction(const VulkanHandleType handle) {
        m_deferredDestructors.push_back(
            {m_frameIndex + m_framesInFlight, handle, [](void* handle, VulkanResourceDeallocator* deallocator) {
                 getDestroyFunc<VulkanHandleType>()(
                     deallocator->getDeviceHandle(), static_cast<VulkanHandleType>(handle), nullptr);
             }});
    }

    void deferMemoryDeallocation(const VmaAllocation allocation) {
        m_deferredMemoryDeallocations.emplace_back(m_frameIndex + m_framesInFlight, allocation);
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

    uint32_t getFramesInFlight() const {
        return m_framesInFlight;
    }

private:
    VkDevice m_deviceHandle{VK_NULL_HANDLE};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    uint64_t m_frameIndex{0};
    uint32_t m_framesInFlight{0};

    std::vector<DeferredHandleDestructor> m_deferredDestructors;
    std::vector<DeferredMemoryDestructor> m_deferredMemoryDeallocations;
};
} // namespace crisp