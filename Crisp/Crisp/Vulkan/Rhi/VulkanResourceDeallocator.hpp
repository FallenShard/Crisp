#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
class VulkanResourceDeallocator;
using VulkanDestructorCallback = void (*)(void*, VulkanResourceDeallocator*);

struct DeferredDestructor {
    int64_t deferredFrameIndex;
    int32_t framesRemaining;
    void* vulkanHandle;
    VulkanDestructorCallback destructorCallback;
};

class VulkanResourceDeallocator {
public:
    explicit VulkanResourceDeallocator(VkDevice device, VmaAllocator allocator, int32_t virtualFrameCount);
    ~VulkanResourceDeallocator();

    VulkanResourceDeallocator(const VulkanResourceDeallocator&) = delete;
    VulkanResourceDeallocator& operator=(const VulkanResourceDeallocator&) = delete;

    VulkanResourceDeallocator(VulkanResourceDeallocator&&) noexcept = default;
    VulkanResourceDeallocator& operator=(VulkanResourceDeallocator&&) noexcept = default;

    void decrementLifetimes();

    void freeAllResources();

    template <typename VulkanHandleType>
    void deferDestruction(int32_t framesToLive, VulkanHandleType handle, VulkanDestructorCallback callback) {
        m_deferredDestructors.push_back({0, framesToLive, handle, callback});
    }

    template <typename VulkanHandleType>
    void deferDestruction(int32_t framesToLive, VulkanHandleType handle) {
        m_deferredDestructors.push_back(
            {0, framesToLive, handle, [](void* handle, VulkanResourceDeallocator* deallocator) {
                 getDestroyFunc<VulkanHandleType>()(
                     deallocator->getDeviceHandle(), static_cast<VulkanHandleType>(handle), nullptr);
             }});
    }

    void deferMemoryDeallocation(int32_t framesToLive, VmaAllocation allocation) {
        m_deferredVmaDeallocations.emplace_back(framesToLive, allocation);
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

private:
    VkDevice m_deviceHandle{VK_NULL_HANDLE};
    VmaAllocator m_allocator{VK_NULL_HANDLE};

    int32_t m_virtualFrameCount{1};

    std::vector<DeferredDestructor> m_deferredDestructors;
    std::vector<std::pair<int32_t, VmaAllocation>> m_deferredVmaDeallocations;
};
} // namespace crisp