#pragma once

#include <Crisp/Vulkan/VulkanMemoryHeap.hpp>

#include <cstdint>
#include <memory>

namespace crisp
{
    class VulkanPhysicalDevice;

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
        VulkanMemoryAllocator(const VulkanPhysicalDevice& physicalDevice, VkDevice deviceHandle);

        VulkanMemoryHeap* getHeapFromMemProps(VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const;
        VulkanMemoryHeap* getDeviceBufferHeap() const;
        VulkanMemoryHeap* getDeviceImageHeap() const;
        VulkanMemoryHeap* getStagingBufferHeap() const;

        DeviceMemoryMetrics getDeviceMemoryUsage() const;

    private:
        const VulkanPhysicalDevice* m_physicalDevice;

        std::unique_ptr<VulkanMemoryHeap> m_deviceBufferHeap;
        std::unique_ptr<VulkanMemoryHeap> m_deviceImageHeap;
        std::unique_ptr<VulkanMemoryHeap> m_stagingBufferHeap;
    };
}
