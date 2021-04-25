#include "VulkanMemoryAllocator.hpp"

#include "VulkanContext.hpp"
#include "VulkanDevice.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <stdexcept>

namespace crisp
{
    namespace
    {
        static constexpr VkDeviceSize DeviceHeapSize = 512 << 20; // 256 MB
        static constexpr VkDeviceSize StagingHeapSize = 512 << 20; // 256 MB

        auto logger = spdlog::stdout_color_mt("VulkanMemoryAllocator");
    }

    VulkanMemoryAllocator::VulkanMemoryAllocator(const VulkanContext& context, VkDevice deviceHandle)
        : m_context(context)
    {
        // Device buffer memory
        std::optional<uint32_t> deviceBufferHeapIndex = m_context.findDeviceBufferMemoryType(deviceHandle);
        if (!deviceBufferHeapIndex.has_value())
            throw std::runtime_error("Requested device buffer heap memory is not available!");
        m_deviceBufferHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, deviceBufferHeapIndex.value(), deviceHandle, "Device Buffer Heap");

        // Device image memory
        std::optional<uint32_t> deviceImageHeapIndex = m_context.findDeviceImageMemoryType(deviceHandle);
        if (!deviceImageHeapIndex.has_value())
            throw std::runtime_error("Requested device image heap memory is not available!");
        m_deviceImageHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, deviceImageHeapIndex.value(), deviceHandle, "Device Image Heap");

        // Staging memory
        std::optional<uint32_t> stagingBufferHeapIndex = m_context.findStagingBufferMemoryType(deviceHandle);
        if (!stagingBufferHeapIndex.has_value())
            throw std::runtime_error("Requested staging buffer heap memory is not available!");
        m_stagingBufferHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingHeapSize, stagingBufferHeapIndex.value(), deviceHandle, "Staging Buffer Heap");

    }

    VulkanMemoryHeap* VulkanMemoryAllocator::getHeapFromMemProps(VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const
    {
        std::optional<uint32_t> supportedHeapIndex = m_context.findMemoryType(memoryTypeBits, flags);

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

    void VulkanMemoryAllocator::printMemoryStatus()
    {
    }

    DeviceMemoryMetrics VulkanMemoryAllocator::getDeviceMemoryUsage()
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
