#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanMemoryHeap.hpp>

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
    explicit VulkanResourceDeallocator(VkDevice device, int32_t virtualFrameCount);
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

    void deferMemoryDeallocation(int32_t framesToLive, VulkanMemoryHeap::Allocation allocation) {
        m_deferredDeallocations.emplace_back(framesToLive, allocation);
    }

    // Object tags useful for debugging
    void setTag(void* handle, std::string tag) {
        m_handleTagMap.emplace(handle, std::move(tag));
    }

    const std::string& getTag(void* handle) const {
        if (const auto iter = m_handleTagMap.find(handle); iter != m_handleTagMap.end()) {
            return iter->second;
        }

        return m_handleTagMap.at(VK_NULL_HANDLE);
    }

    void removeTag(void* handle) {
        m_handleTagMap.erase(handle);
    }

    VkDevice getDeviceHandle() const {
        return m_deviceHandle;
    }

    size_t getDeferredDestructorCount() const {
        return m_deferredDestructors.size();
    }

private:
    VkDevice m_deviceHandle{VK_NULL_HANDLE};

    int32_t m_virtualFrameCount{1};

    FlatHashMap<void*, std::string> m_handleTagMap;

    std::vector<DeferredDestructor> m_deferredDestructors;
    std::vector<std::pair<int32_t, VulkanMemoryHeap::Allocation>> m_deferredDeallocations;
};
} // namespace crisp