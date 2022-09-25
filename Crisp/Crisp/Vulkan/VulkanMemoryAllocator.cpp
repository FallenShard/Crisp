#include <Crisp/Vulkan/VulkanMemoryAllocator.hpp>

#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>

#include <Crisp/Common/Logger.hpp>

#include <stdexcept>

namespace crisp
{
namespace
{
static constexpr VkDeviceSize DeviceHeapSize = 512 << 20;  // 512 MB
static constexpr VkDeviceSize StagingHeapSize = 512 << 20; // 512 MB

auto logger = spdlog::stdout_color_mt("VulkanMemoryAllocator");
} // namespace

VulkanMemoryAllocator::VulkanMemoryAllocator(const VulkanPhysicalDevice& physicalDevice, VkDevice deviceHandle)
    : m_physicalDevice(&physicalDevice)
{
    // Device buffer memory
    const uint32_t deviceBufferHeapIndex = m_physicalDevice->findDeviceBufferMemoryType(deviceHandle).unwrap();
    m_deviceBufferHeap = std::make_unique<VulkanMemoryHeap>(
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, deviceBufferHeapIndex, deviceHandle, "Device Buffer Heap");

    // Device image memory
    const uint32_t deviceImageHeapIndex = m_physicalDevice->findDeviceImageMemoryType(deviceHandle).unwrap();
    m_deviceImageHeap = std::make_unique<VulkanMemoryHeap>(
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, deviceImageHeapIndex, deviceHandle, "Device Image Heap");

    // Staging memory
    const uint32_t stagingBufferHeapIndex = m_physicalDevice->findStagingBufferMemoryType(deviceHandle).unwrap();
    m_stagingBufferHeap = std::make_unique<VulkanMemoryHeap>(
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        StagingHeapSize,
        stagingBufferHeapIndex,
        deviceHandle,
        "Staging Buffer Heap");
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
    memoryMetrics.bufferMemorySize = m_deviceBufferHeap->getAllocatedSize();
    memoryMetrics.bufferMemoryUsed = m_deviceBufferHeap->getUsedSize();
    memoryMetrics.imageMemorySize = m_deviceImageHeap->getAllocatedSize();
    memoryMetrics.imageMemoryUsed = m_deviceImageHeap->getUsedSize();
    memoryMetrics.stagingMemorySize = m_stagingBufferHeap->getAllocatedSize();
    memoryMetrics.stagingMemoryUsed = m_stagingBufferHeap->getUsedSize();
    return memoryMetrics;
}

Result<VulkanMemoryHeap::Allocation> VulkanMemoryAllocator::allocateBuffer(
    VkMemoryPropertyFlags memoryProperties, const VkMemoryRequirements& memoryRequirements)
{
    const uint32_t supportedHeapIndex =
        m_physicalDevice->findMemoryType(memoryRequirements.memoryTypeBits, memoryProperties).unwrap();

    if (m_deviceBufferHeap->isFromHeapIndex(supportedHeapIndex, memoryProperties))
    {
        return m_deviceBufferHeap->allocate(memoryRequirements.size, memoryRequirements.alignment);
    }
    else if (m_stagingBufferHeap->isFromHeapIndex(supportedHeapIndex, memoryProperties))
    {
        return m_stagingBufferHeap->allocate(memoryRequirements.size, memoryRequirements.alignment);
    }

    return resultError("Failed to get a heap from specified properties when allocating a buffer!");
}

Result<VulkanMemoryHeap::Allocation> VulkanMemoryAllocator::allocateImage(
    VkMemoryPropertyFlags memoryProperties, const VkMemoryRequirements& memoryRequirements)
{
    const uint32_t supportedHeapIndex =
        m_physicalDevice->findMemoryType(memoryRequirements.memoryTypeBits, memoryProperties).unwrap();

    if (m_deviceImageHeap->isFromHeapIndex(supportedHeapIndex, memoryProperties))
    {
        return m_deviceImageHeap->allocate(memoryRequirements.size, memoryRequirements.alignment);
    }

    return resultError("Failed to get a heap from specified properties when allocating an image!");
}
} // namespace crisp
