#pragma once

#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>

#include <CrispCore/RobinHood.hpp>

#include <vulkan/vulkan.h>

namespace crisp
{
class VulkanResourceDeallocator;
using VulkanDestructorCallback = void (*)(void*, VulkanResourceDeallocator*);
struct DeferredDestructor
{
    int64_t deferredFrameIndex;
    int32_t framesRemaining;
    void* vulkanHandle;
    VulkanDestructorCallback destructorCallback;
};

class VulkanResourceDeallocator
{
public:
    explicit VulkanResourceDeallocator(VkDevice device, int32_t virtualFrameCount);
    ~VulkanResourceDeallocator();

    void incrementFrameCount();

    void freeAllResources();

    template <typename VulkanHandleType>
    void deferDestruction(int32_t framesToLive, VulkanHandleType handle, VulkanDestructorCallback callback)
    {
        m_deferredDestructors.push_back({ m_currentFrameIndex, framesToLive, handle, callback });
    }

    void deferMemoryDeallocation(int32_t framesToLive, VulkanMemoryHeap::Allocation allocation)
    {
        m_deferredDeallocations.emplace_back(framesToLive, allocation);
    }

    // Object tags useful for debugging
    inline void setTag(void* handle, std::string tag)
    {
        m_handleTagMap.emplace(handle, std::move(tag));
    }

    inline const std::string& getTag(void* handle) const
    {
        if (const auto iter = m_handleTagMap.find(handle); iter != m_handleTagMap.end())
            return iter->second;

        return m_handleTagMap.at(VK_NULL_HANDLE);
    }

    inline void removeTag(void* handle)
    {
        m_handleTagMap.erase(handle);
    }

    inline VkDevice getDeviceHandle() const
    {
        return m_deviceHandle;
    }

    inline size_t getDeferredDestructorCount() const
    {
        return m_deferredDestructors.size();
    }

private:
    VkDevice m_deviceHandle{ VK_NULL_HANDLE };

    int64_t m_currentFrameIndex{ -1 };
    int32_t m_virtualFrameCount{ 1 };

    robin_hood::unordered_flat_map<void*, std::string> m_handleTagMap;

    std::vector<DeferredDestructor> m_deferredDestructors;
    std::vector<std::pair<int32_t, VulkanMemoryHeap::Allocation>> m_deferredDeallocations;
};
} // namespace crisp