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

    VulkanMemoryHeap* VulkanMemoryAllocator::getHeapFromMemProps(VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const
    {
        const uint32_t supportedHeapIndex = m_physicalDevice->findMemoryType(memoryTypeBits, flags).unwrap();

        VulkanMemoryHeap* heap = nullptr;
        if (supportedHeapIndex == m_deviceBufferHeap->memoryTypeIndex)
        {
            heap = m_deviceBufferHeap.get();
        }
        else if (supportedHeapIndex == m_stagingBufferHeap->memoryTypeIndex)
        {
            heap = m_stagingBufferHeap.get();
        }
        else
        {
            logger->critical("Wrong heap type specified when allocating memory!");
            return nullptr;
        }

        if (heap->properties != flags)
        {
            logger->critical("Wrong heap type specified when allocating memory!");
            return nullptr;
        }

        return heap;
    }

    VulkanMemoryHeap* VulkanMemoryAllocator::getDeviceBufferHeap() const
    {
        return m_deviceBufferHeap.get();
    }

    VulkanMemoryHeap* VulkanMemoryAllocator::getDeviceImageHeap() const
    {
        return m_deviceImageHeap.get();
    }

    VulkanMemoryHeap* VulkanMemoryAllocator::getStagingBufferHeap() const
    {
        return m_stagingBufferHeap.get();
    }

    DeviceMemoryMetrics VulkanMemoryAllocator::getDeviceMemoryUsage() const
    {
        DeviceMemoryMetrics memoryMetrics = {};
        memoryMetrics.bufferMemorySize  = m_deviceBufferHeap->getTotalAllocatedSize();
        memoryMetrics.bufferMemoryUsed  = m_deviceBufferHeap->usedSize;
        memoryMetrics.imageMemorySize   = m_deviceImageHeap->getTotalAllocatedSize();
        memoryMetrics.imageMemoryUsed   = m_deviceImageHeap->usedSize;
        memoryMetrics.stagingMemorySize = m_stagingBufferHeap->getTotalAllocatedSize();
        memoryMetrics.stagingMemoryUsed = m_stagingBufferHeap->usedSize;
        return memoryMetrics;
    }
}
