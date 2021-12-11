#include <Crisp/Vulkan/VulkanMemoryAllocator.hpp>

#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <stdexcept>

namespace crisp
{
    namespace
    {
        static constexpr VkDeviceSize DeviceHeapSize = 512 << 20; // 512 MB
        static constexpr VkDeviceSize StagingHeapSize = 512 << 20; // 512 MB

        auto logger = spdlog::stdout_color_mt("VulkanMemoryAllocator");
    }

    VulkanMemoryAllocator::VulkanMemoryAllocator(const VulkanPhysicalDevice& physicalDevice, VkDevice deviceHandle)
        : m_physicalDevice(&physicalDevice)
    {
        // Device buffer memory
        const uint32_t deviceBufferHeapIndex = m_physicalDevice->findDeviceBufferMemoryType(deviceHandle).unwrap();
        m_deviceBufferHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, deviceBufferHeapIndex, deviceHandle, "Device Buffer Heap");

        // Device image memory
        const uint32_t deviceImageHeapIndex = m_physicalDevice->findDeviceImageMemoryType(deviceHandle).unwrap();
        m_deviceImageHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, deviceImageHeapIndex, deviceHandle, "Device Image Heap");

        // Staging memory
        const uint32_t stagingBufferHeapIndex = m_physicalDevice->findStagingBufferMemoryType(deviceHandle).unwrap();
        m_stagingBufferHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingHeapSize, stagingBufferHeapIndex, deviceHandle, "Staging Buffer Heap");
    }

    Result<VulkanMemoryHeap*> VulkanMemoryAllocator::getHeapFromMemProps(VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const
    {
        const uint32_t supportedHeapIndex = m_physicalDevice->findMemoryType(memoryTypeBits, flags).unwrap();

        if (m_deviceImageHeap->isFromHeapIndex(supportedHeapIndex, flags))
        {
            return m_deviceImageHeap.get();
        }

        if (m_deviceBufferHeap->isFromHeapIndex(supportedHeapIndex, flags))
        {
            return m_deviceBufferHeap.get();
        }

        if (m_stagingBufferHeap->isFromHeapIndex(supportedHeapIndex, flags))
        {
            return m_stagingBufferHeap.get();
        }

        return resultError("Failed to get a heap from specified properties!");
    }

    VulkanMemoryHeap& VulkanMemoryAllocator::getDeviceBufferHeap() const
    {
        return *m_deviceBufferHeap;
    }

    VulkanMemoryHeap& VulkanMemoryAllocator::getDeviceImageHeap() const
    {
        return *m_deviceImageHeap;
    }

    VulkanMemoryHeap& VulkanMemoryAllocator::getStagingBufferHeap() const
    {
        return *m_stagingBufferHeap;
    }

    DeviceMemoryMetrics VulkanMemoryAllocator::getDeviceMemoryUsage() const
    {
        DeviceMemoryMetrics memoryMetrics = {};
        memoryMetrics.bufferMemorySize  = m_deviceBufferHeap->getAllocatedSize();
        memoryMetrics.bufferMemoryUsed  = m_deviceBufferHeap->getUsedSize();
        memoryMetrics.imageMemorySize   = m_deviceImageHeap->getAllocatedSize();
        memoryMetrics.imageMemoryUsed   = m_deviceImageHeap->getUsedSize();
        memoryMetrics.stagingMemorySize = m_stagingBufferHeap->getAllocatedSize();
        memoryMetrics.stagingMemoryUsed = m_stagingBufferHeap->getUsedSize();
        return memoryMetrics;
    }

    Result<VulkanMemoryHeap::Allocation> VulkanMemoryAllocator::allocate(VkMemoryPropertyFlags memoryProperties, const VkMemoryRequirements& memoryRequirements)
    {
        auto heap = getHeapFromMemProps(memoryProperties, memoryRequirements.memoryTypeBits).unwrap();
        return heap->allocate(memoryRequirements.size, memoryRequirements.alignment);
    }
}
