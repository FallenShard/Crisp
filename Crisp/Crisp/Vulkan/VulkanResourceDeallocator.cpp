#include <Crisp/Vulkan/VulkanResourceDeallocator.hpp>

namespace crisp
{
VulkanResourceDeallocator::VulkanResourceDeallocator(VkDevice device, int32_t virtualFrameCount)
    : m_deviceHandle(device)
    , m_virtualFrameCount{ virtualFrameCount }
{
    m_handleTagMap.emplace(VK_NULL_HANDLE, "Unknown");
}

VulkanResourceDeallocator::~VulkanResourceDeallocator()
{
    freeAllResources();
}

void VulkanResourceDeallocator::incrementFrameCount()
{
    ++m_currentFrameIndex;

    // Handle deferred destructors
    for (auto& destructor : m_deferredDestructors)
    {
        if (--destructor.framesRemaining < 0)
        {
            destructor.destructorCallback(destructor.vulkanHandle, this);
        }
    }

    m_deferredDestructors.erase(std::remove_if(m_deferredDestructors.begin(), m_deferredDestructors.end(),
                                    [](const auto& a)
                                    {
                                        return a.framesRemaining < 0;
                                    }),
        m_deferredDestructors.end());

    // Free the memory chunks
    for (auto& memoryChunkPair : m_deferredDeallocations)
    {
        if (--memoryChunkPair.first < 0)
            memoryChunkPair.second.free();
    }

    m_deferredDeallocations.erase(std::remove_if(m_deferredDeallocations.begin(), m_deferredDeallocations.end(),
                                      [](const auto& a)
                                      {
                                          return a.first < 0;
                                      }),
        m_deferredDeallocations.end());
}

void VulkanResourceDeallocator::freeAllResources()
{
    for (auto& destructor : m_deferredDestructors)
        destructor.destructorCallback(destructor.vulkanHandle, this);
    m_deferredDestructors.clear();

    for (auto& memoryChunkPair : m_deferredDeallocations)
        memoryChunkPair.second.free();
    m_deferredDeallocations.clear();
}
} // namespace crisp
