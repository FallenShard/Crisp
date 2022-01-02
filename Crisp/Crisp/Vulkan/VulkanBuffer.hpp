#pragma once

#include <Crisp/Vulkan/VulkanBufferView.hpp>
#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
class VulkanDevice;

class VulkanBuffer : public VulkanResource<VkBuffer, vkDestroyBuffer>
{
public:
    VulkanBuffer(const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memProps);
    ~VulkanBuffer();

    VkDeviceSize getSize() const;

    void copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer);
    void copyFrom(VkCommandBuffer cmdBuffer, const VulkanBuffer& srcBuffer, VkDeviceSize srcOffset,
        VkDeviceSize dstOffset, VkDeviceSize size);

    VulkanBufferSpan createSpan() const;

protected:
    VulkanMemoryHeap::Allocation m_allocation;
    VkDeviceSize m_size;
};

class StagingVulkanBuffer final : public VulkanBuffer
{
public:
    StagingVulkanBuffer(VulkanDevice& device, VkDeviceSize size,
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    void updateFromHost(const void* hostMemoryData, VkDeviceSize size, VkDeviceSize offset);
    void updateFromHost(const void* hostMemoryData);

    template <typename T>
    inline void updateFromHost(const std::vector<T>& buffer)
    {
        updateFromHost(buffer.data(), buffer.size() * sizeof(T), 0);
    }

    void updateFromStaging(const StagingVulkanBuffer& stagingVulkanBuffer);

    template <typename T>
    inline const T* getHostVisibleData() const
    {
        return reinterpret_cast<const T*>(m_allocation.getMappedPtr());
    }

private:
    VulkanDevice* m_device;
};
} // namespace crisp