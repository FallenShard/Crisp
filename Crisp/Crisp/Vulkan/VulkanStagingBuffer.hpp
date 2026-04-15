#pragma once

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {

struct StagingAllocation {
    VkDeviceSize offset; // Byte offset within the staging buffer.
    VkDeviceSize size;   // Requested size (before alignment padding).
    void* mappedPtr;     // Host-visible pointer at this offset.
};

class VulkanStagingBuffer {
public:
    // Creates a persistent-mapped host-visible buffer of `capacity` bytes.
    // `alignment` is the minimum suballocation alignment (e.g. optimalBufferCopyOffsetAlignment).
    VulkanStagingBuffer(VulkanDevice& device, VkDeviceSize capacity, VkDeviceSize alignment);

    // Suballocate `size` bytes. Returns std::nullopt if the ring is full.
    std::optional<StagingAllocation> allocate(VkDeviceSize size);

    // Reclaim all memory up to and including `reclaimOffset`.
    void reclaim(VkDeviceSize reclaimOffset);

    // Reclaim everything (e.g. after vkDeviceWaitIdle).
    void reclaimAll();

    VkBuffer getHandle() const;
    VkDeviceSize getCapacity() const;
    VkDeviceSize getHead() const;
    VkDeviceSize getFreeBytes() const;

private:
    VkDeviceSize alignUp(VkDeviceSize value) const;

    std::unique_ptr<StagingVulkanBuffer> m_buffer;
    VkDeviceSize m_capacity;
    VkDeviceSize m_alignment;
    VkDeviceSize m_head{0};
    VkDeviceSize m_tail{0};
    bool m_empty{true}; // Disambiguates head == tail (empty vs full).
};

} // namespace crisp
