#pragma once

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {
class VulkanBuffer : public VulkanResource<VkBuffer> {
public:
    VulkanBuffer(
        const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memProps);
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

protected:
    VmaAllocation m_allocation;
    VmaAllocationInfo m_allocationInfo;
    VkDeviceSize m_size;
    VkDeviceAddress m_address;
};

class StagingVulkanBuffer final : public VulkanBuffer {
public:
    StagingVulkanBuffer(
        VulkanDevice& device,
        VkDeviceSize size,
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    void updateFromHost(const void* hostMemoryData, VkDeviceSize size, VkDeviceSize offset);
    void updateFromHost(const void* hostMemoryData);

    template <typename T>
    void updateFromHost(const std::vector<T>& buffer) {
        updateFromHost(buffer.data(), buffer.size() * sizeof(T), 0);
    }

    template <typename T, size_t N>
    void updateFromHost(const std::array<T, N>& buffer) {
        updateFromHost(buffer.data(), buffer.size() * sizeof(T), 0);
    }

    void updateFromStaging(const StagingVulkanBuffer& stagingVulkanBuffer);

    template <typename T>
    const T* getHostVisibleData() const {
        return reinterpret_cast<const T*>(m_allocationInfo.pMappedData); // NOLINT
    }

    const VulkanDevice& getDevice() const {
        return *m_device;
    }

private:
    VulkanDevice* m_device;
};

inline std::unique_ptr<VulkanBuffer> createStorageBuffer(
    const VulkanDevice& device, const VkDeviceSize size, const VkBufferUsageFlags additionalUsageFlags = 0) {
    return std::make_unique<VulkanBuffer>(
        device, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | additionalUsageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

template <typename T>
inline std::unique_ptr<VulkanBuffer> createStorageBuffer(
    VulkanDevice& device, const std::vector<T>& data, const VkBufferUsageFlags additionalUsageFlags = 0) {
    auto buffer = std::make_unique<VulkanBuffer>(
        device,
        data.size() * sizeof(T),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | additionalUsageFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto stagingBuffer = std::make_unique<StagingVulkanBuffer>(device, data.size() * sizeof(T));
    stagingBuffer->updateFromHost(data);

    device.getGeneralQueue().submitAndWait([&buffer, &stagingBuffer, &data](const VkCommandBuffer cmdBuffer) {
        buffer->copyFrom(cmdBuffer, *stagingBuffer);
    });
    return buffer;
}
} // namespace crisp