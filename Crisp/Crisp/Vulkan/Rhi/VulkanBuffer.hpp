#pragma once

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {

enum class BufferMemoryType : uint8_t {
    // Device-local memory. Not host-visible. Use with TRANSFER_DST + a staging upload, or BDA writes.
    GpuOnly,
    // Host-visible memory optimized for sequential CPU writes (typically write-combined).
    // Use for staging-source buffers that the CPU writes once and the GPU reads via vkCmdCopy*.
    HostUpload,
    // Host-visible memory optimized for cached host reads (random access).
    // Use for staging-destination buffers populated by the GPU and then read by the CPU.
    HostReadback,
};

class VulkanBuffer : public VulkanResource<VkBuffer> {
public:
    VulkanBuffer(
        const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usageFlags, BufferMemoryType memoryType);
    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    VkDeviceSize getSize() const;
    VkDeviceAddress getDeviceAddress() const;

    void copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer) const;
    void copyFrom(
        VkCommandBuffer cmdBuffer,
        const VulkanBuffer& srcBuffer,
        VkDeviceSize srcOffset,
        VkDeviceSize dstOffset,
        VkDeviceSize size) const;

    VkDescriptorBufferInfo createDescriptorInfo(VkDeviceSize offset, VkDeviceSize size) const;
    VkDescriptorBufferInfo createDescriptorInfo() const;

    void updateFromHost(const void* data, VkDeviceSize size, VkDeviceSize offset);

    template <typename T>
    void updateFromHost(const std::vector<T>& buffer) {
        updateFromHost(buffer.data(), buffer.size() * sizeof(T), 0);
    }

    template <typename T, size_t N>
    void updateFromHost(const std::array<T, N>& buffer) {
        updateFromHost(buffer.data(), buffer.size() * sizeof(T), 0);
    }

    template <typename T>
    const T* getHostVisibleData() const {
        return static_cast<const T*>(m_allocationInfo.pMappedData);
    }

    template <typename T>
    T* getHostVisibleData() {
        return static_cast<T*>(m_allocationInfo.pMappedData);
    }

private:
    VmaAllocator m_allocator;
    VmaAllocation m_allocation;
    VmaAllocationInfo m_allocationInfo;
    VkDeviceSize m_size;
    VkDeviceAddress m_address;
};

inline std::unique_ptr<VulkanBuffer> createStorageBuffer(
    const VulkanDevice& device, const VkDeviceSize size, const VkBufferUsageFlags additionalUsageFlags = 0) {
    return std::make_unique<VulkanBuffer>(
        device,
        size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | additionalUsageFlags,
        BufferMemoryType::GpuOnly);
}

} // namespace crisp