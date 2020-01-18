#pragma once

#include <map>
#include <set>
#include <memory>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>
#include "VulkanMemoryHeap.hpp"

namespace crisp
{
    class VulkanContext;
    class VulkanQueue;

    struct DeviceMemoryMetrics
    {
        uint64_t bufferMemorySize;
        uint64_t bufferMemoryUsed;
        uint64_t imageMemorySize;
        uint64_t imageMemoryUsed;
        uint64_t stagingMemorySize;
        uint64_t stagingMemoryUsed;
    };

    class VulkanDevice
    {
    public:
        VulkanDevice(VulkanContext* vulkanContext);
        ~VulkanDevice();

        VkDevice getHandle() const;
        const VulkanQueue* getGeneralQueue() const;
        const VulkanQueue* getTransferQueue() const;
        VulkanContext* getContext() const;

        void invalidateMappedRange(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
        void flushMappedRanges();

        VkSemaphore createSemaphore() const;
        VkFence createFence(VkFenceCreateFlags flags) const;

        void printMemoryStatus();
        DeviceMemoryMetrics getDeviceMemoryUsage();

        VulkanMemoryHeap* getHeapFromMemProps(VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const;
        VulkanMemoryHeap* getDeviceBufferHeap() const;
        VulkanMemoryHeap* getDeviceImageHeap() const;
        VulkanMemoryHeap* getStagingBufferHeap() const;

        void postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorBufferInfo bufferInfo);
        void postDescriptorWrite(VkWriteDescriptorSet&& write, std::vector<VkDescriptorBufferInfo>&& bufferInfos);
        void postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorImageInfo imageInfo);
        void flushDescriptorUpdates();

    private:
        static constexpr VkDeviceSize DeviceHeapSize  = 256 << 20; // 256 MB
        static constexpr VkDeviceSize StagingHeapSize = 256 << 20; // 256 MB

        VulkanContext* m_context;

        VkDevice m_device;
        std::unique_ptr<VulkanQueue> m_generalQueue;
        std::unique_ptr<VulkanQueue> m_transferQueue;

        std::unique_ptr<VulkanMemoryHeap> m_deviceBufferHeap;
        std::unique_ptr<VulkanMemoryHeap> m_deviceImageHeap;
        std::unique_ptr<VulkanMemoryHeap> m_stagingBufferHeap;

        std::vector<VkMappedMemoryRange> m_unflushedRanges;

        std::list<std::vector<VkDescriptorBufferInfo>> m_bufferInfos;
        std::list<VkDescriptorImageInfo>  m_imageInfos;
        std::vector<VkWriteDescriptorSet> m_descriptorWrites;
    };
}