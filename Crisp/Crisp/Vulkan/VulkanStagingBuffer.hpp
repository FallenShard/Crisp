#pragma once

#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>

namespace crisp {

struct StagingAllocation {
    VkBuffer buffer;     // Source buffer this allocation lives in.
    VkDeviceSize offset; // Byte offset within the staging buffer.
    VkDeviceSize size;   // Requested size (before alignment padding).
    void* mappedPtr;     // Host-visible pointer at this offset.
};

class VulkanQueue;

std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanDevice& device, const void* data, VkDeviceSize size);

void uploadBufferBlocking(
    VulkanDevice& device,
    const VulkanQueue& queue,
    const VulkanBuffer& dstBuffer,
    VkDeviceSize dstOffset,
    const void* data,
    VkDeviceSize size);

void uploadBufferBlocking(
    VulkanDevice& device, const VulkanQueue& queue, const VulkanBuffer& dstBuffer, const void* data, VkDeviceSize size);

template <typename T>
void uploadBufferBlocking(
    VulkanDevice& device, const VulkanQueue& queue, const VulkanBuffer& dstBuffer, const std::vector<T>& data) {
    uploadBufferBlocking(device, queue, dstBuffer, data.data(), data.size() * sizeof(T));
}

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

    std::unique_ptr<VulkanBuffer> m_buffer;
    VkDeviceSize m_capacity;
    VkDeviceSize m_alignment;
    VkDeviceSize m_head{0};
    VkDeviceSize m_tail{0};
    bool m_empty{true}; // Disambiguates head == tail (empty vs full).
};

} // namespace crisp
