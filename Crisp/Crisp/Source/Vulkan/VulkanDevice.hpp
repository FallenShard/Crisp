#pragma once

#include <map>
#include <set>
#include <memory>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>
#include "MemoryHeap.hpp"

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

        VkSampler createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode);

        VkSemaphore createSemaphore() const;
        VkFence createFence(VkFenceCreateFlags flags) const;

        void printMemoryStatus();
        DeviceMemoryMetrics getDeviceMemoryUsage();

        MemoryHeap* getHeapFromMemProps(VkBuffer buffer, VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const;
        MemoryHeap* getDeviceBufferHeap() const;
        MemoryHeap* getDeviceImageHeap() const;
        MemoryHeap* getStagingBufferHeap() const;
        void* getStagingMemoryPtr() const;

    private:
        static constexpr VkDeviceSize DeviceHeapSize  = 256 << 20; // 256 MB
        static constexpr VkDeviceSize StagingHeapSize = 128 << 20; // 128 MB

        VulkanContext* m_context;

        VkDevice m_device;
        std::unique_ptr<VulkanQueue> m_generalQueue;
        std::unique_ptr<VulkanQueue> m_transferQueue;

        std::unique_ptr<MemoryHeap> m_deviceBufferHeap;
        std::unique_ptr<MemoryHeap> m_deviceImageHeap;
        std::unique_ptr<MemoryHeap> m_stagingBufferHeap;
        void* m_mappedStagingPtr;

        std::vector<VkMappedMemoryRange> m_unflushedRanges;
    };
}