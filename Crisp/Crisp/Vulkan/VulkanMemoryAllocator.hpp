#pragma once

#include <cstdint>
#include <memory>

#include "VulkanMemoryHeap.hpp"


namespace crisp
{
    class VulkanContext;

    struct DeviceMemoryMetrics
    {
        uint64_t bufferMemorySize;
        uint64_t bufferMemoryUsed;
        uint64_t imageMemorySize;
        uint64_t imageMemoryUsed;
        uint64_t stagingMemorySize;
        uint64_t stagingMemoryUsed;
    };

    class VulkanMemoryAllocator
    {
    public:
        VulkanMemoryAllocator(const VulkanContext& context, VkDevice deviceHandle);

        VulkanMemoryHeap* getHeapFromMemProps(VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const;
        VulkanMemoryHeap* getDeviceBufferHeap() const;
        VulkanMemoryHeap* getDeviceImageHeap() const;
        VulkanMemoryHeap* getStagingBufferHeap() const;

        void printMemoryStatus();
        DeviceMemoryMetrics getDeviceMemoryUsage();

    private:
        const VulkanContext& m_context;

        std::unique_ptr<VulkanMemoryHeap> m_deviceBufferHeap;
        std::unique_ptr<VulkanMemoryHeap> m_deviceImageHeap;
        std::unique_ptr<VulkanMemoryHeap> m_stagingBufferHeap;
    };
}
