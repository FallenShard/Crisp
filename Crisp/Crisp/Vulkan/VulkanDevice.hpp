#pragma once

#include <Crisp/Vulkan/VulkanMemoryAllocator.hpp>
#include <Crisp/Vulkan/VulkanResourceDeallocator.hpp>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanPhysicalDevice;
    struct VulkanQueueConfiguration;
    class VulkanQueue;

    class VulkanDevice
    {
    public:
        VulkanDevice(const VulkanPhysicalDevice& physicalDevice, const VulkanQueueConfiguration& queueConfig, int32_t virtualFrameCount);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice(VulkanDevice&&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;
        VulkanDevice& operator=(VulkanDevice&&) = delete;

        VkDevice getHandle() const;
        const VulkanPhysicalDevice& getPhysicalDevice() const;
        const VulkanQueue& getGeneralQueue() const;
        const VulkanQueue& getComputeQueue() const;
        const VulkanQueue& getTransferQueue() const;

        void invalidateMappedRange(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
        void flushMappedRanges();

        VkSemaphore createSemaphore() const;
        VkFence createFence(VkFenceCreateFlags flags) const;
        VkBuffer createBuffer(const VkBufferCreateInfo& bufferCreateInfo) const;

        VulkanMemoryAllocator& getMemoryAllocator() const { return *m_memoryAllocator; }

        void postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorBufferInfo bufferInfo);
        void postDescriptorWrite(VkWriteDescriptorSet&& write, std::vector<VkDescriptorBufferInfo>&& bufferInfos);
        void postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorImageInfo imageInfo);
        void flushDescriptorUpdates();

        VulkanResourceDeallocator& getResourceDeallocator() const { return *m_resourceDeallocator; }

    private:
        VkDevice m_handle;
        const VulkanPhysicalDevice* m_physicalDevice;
        
        std::unique_ptr<VulkanQueue> m_generalQueue;
        std::unique_ptr<VulkanQueue> m_computeQueue;
        std::unique_ptr<VulkanQueue> m_transferQueue;

        std::unique_ptr<VulkanMemoryAllocator> m_memoryAllocator;
        std::vector<VkMappedMemoryRange> m_unflushedRanges;

        std::list<std::vector<VkDescriptorBufferInfo>> m_bufferInfos;
        std::list<VkDescriptorImageInfo>  m_imageInfos;
        std::vector<VkWriteDescriptorSet> m_descriptorWrites;

        std::unique_ptr<VulkanResourceDeallocator> m_resourceDeallocator;
    };
}