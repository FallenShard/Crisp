#pragma once

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanMemoryAllocator.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueueConfiguration.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResourceDeallocator.hpp>
#include <Crisp/Vulkan/VulkanDebugUtils.hpp>

namespace crisp {
class VulkanDevice {
public:
    VulkanDevice(
        const VulkanPhysicalDevice& physicalDevice,
        const VulkanQueueConfiguration& queueConfig,
        int32_t virtualFrameCount);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    VkDevice getHandle() const;
    const VulkanQueue& getGeneralQueue() const;
    const VulkanQueue& getComputeQueue() const;
    const VulkanQueue& getTransferQueue() const;

    void invalidateMappedRange(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
    void flushMappedRanges();

    VkSemaphore createSemaphore() const;
    VkFence createFence(VkFenceCreateFlags flags = 0) const;
    VkBuffer createBuffer(const VkBufferCreateInfo& bufferCreateInfo) const;
    VkImage createImage(const VkImageCreateInfo& imageCreateInfo) const;

    VulkanMemoryAllocator& getMemoryAllocator() const {
        return *m_memoryAllocator;
    }

    void postDescriptorWrite(const VkWriteDescriptorSet& write, const VkDescriptorBufferInfo& bufferInfo);
    void postDescriptorWrite(const VkWriteDescriptorSet& write, std::vector<VkDescriptorBufferInfo>&& bufferInfos);
    void postDescriptorWrite(const VkWriteDescriptorSet& write, const VkDescriptorImageInfo& imageInfo);
    void postDescriptorWrite(const VkWriteDescriptorSet& write);
    void flushDescriptorUpdates();

    VulkanResourceDeallocator& getResourceDeallocator() const {
        return *m_resourceDeallocator;
    }

    inline void wait(const VkFence fence) const {
        vkWaitForFences(m_handle, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    template <size_t N>
    void wait(const std::array<VkFence, N>& fences) const {
        if constexpr (N == 0) {
            return;
        }

        vkWaitForFences(
            m_handle, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    void waitIdle() const;

    const VulkanDebugMarker& getDebugMarker() const {
        return *m_debugMarker;
    }

    void postResourceUpdate(const std::function<void(VkCommandBuffer)>& resourceUpdateFunc);
    void executeResourceUpdates(VkCommandBuffer cmdBuffer);
    void flushResourceUpdates(bool waitOnAllQueues = false);

private:
    VkDevice m_handle;

    VkDeviceSize m_nonCoherentAtomSize;

    std::unique_ptr<VulkanDebugMarker> m_debugMarker;

    std::unique_ptr<VulkanQueue> m_generalQueue;
    std::unique_ptr<VulkanQueue> m_computeQueue;
    std::unique_ptr<VulkanQueue> m_transferQueue;

    std::unique_ptr<VulkanMemoryAllocator> m_memoryAllocator;
    std::vector<VkMappedMemoryRange> m_unflushedRanges;

    std::list<std::vector<VkDescriptorBufferInfo>> m_bufferInfos;
    std::list<VkDescriptorImageInfo> m_imageInfos;
    std::vector<VkWriteDescriptorSet> m_descriptorWrites;

    std::unique_ptr<VulkanResourceDeallocator> m_resourceDeallocator;

    using FunctionVector = std::vector<std::function<void(VkCommandBuffer)>>;
    FunctionVector m_resourceUpdates;
};

VkDevice createLogicalDeviceHandle(const VulkanPhysicalDevice& physicalDevice, const VulkanQueueConfiguration& config);

} // namespace crisp