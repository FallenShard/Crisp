#include "VulkanDevice.hpp"

#include <iostream>

#include "VulkanContext.hpp"
#include "VulkanQueueConfiguration.hpp"
#include "VulkanQueue.hpp"

namespace crisp
{
    VulkanDevice::VulkanDevice(VulkanContext* vulkanContext)
        : m_context(vulkanContext)
        , m_device(VK_NULL_HANDLE)
    {
        VulkanQueueConfiguration config({
            QueueTypeFlags(QueueType::General | QueueType::Present),
            //QueueType::Transfer
        }, m_context);
        m_device = m_context->createLogicalDevice(config);

        m_generalQueue  = std::make_unique<VulkanQueue>(this, config.getQueueIdentifier(0));
        //m_transferQueue = std::make_unique<VulkanQueue>(this, config.getQueueIdentifier(1));

        // Device buffer memory
        std::optional<uint32_t> deviceBufferHeapIndex = m_context->findDeviceBufferMemoryType(m_device);
        if (!deviceBufferHeapIndex.has_value())
            throw std::runtime_error("Requested device buffer heap memory is not available!");
        m_deviceBufferHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize / 2, deviceBufferHeapIndex.value(), m_device, "Device Buffer Heap");

        // Device image memory
        std::optional<uint32_t> deviceImageHeapIndex = m_context->findDeviceImageMemoryType(m_device);
        if (!deviceImageHeapIndex.has_value())
            throw std::runtime_error("Requested device buffer heap memory is not available!");
        m_deviceImageHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, deviceImageHeapIndex.value(), m_device, "Device Image Heap");

        // Staging memory
        std::optional<uint32_t> stagingBufferHeapIndex = m_context->findStagingBufferMemoryType(m_device);
        if (!stagingBufferHeapIndex.has_value())
            throw std::runtime_error("Requested device buffer heap memory is not available!");
        m_stagingBufferHeap = std::make_unique<VulkanMemoryHeap>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingHeapSize, stagingBufferHeapIndex.value(), m_device, "Staging Buffer Heap");
    }

    VulkanDevice::~VulkanDevice()
    {
        m_deviceBufferHeap->freeVulkanMemoryBlocks();
        m_deviceImageHeap->freeVulkanMemoryBlocks();
        m_stagingBufferHeap->freeVulkanMemoryBlocks();

        vkDestroyDevice(m_device, nullptr);
    }

    VkDevice VulkanDevice::getHandle() const
    {
        return m_device;
    }

    const VulkanQueue* VulkanDevice::getGeneralQueue() const
    {
        return m_generalQueue.get();
    }

    const VulkanQueue* VulkanDevice::getTransferQueue() const
    {
        return m_transferQueue.get();
    }

    VulkanContext* VulkanDevice::getContext() const
    {
        return m_context;
    }

    void VulkanDevice::invalidateMappedRange(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
    {
        VkMappedMemoryRange memRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
        memRange.memory = memory;
        memRange.offset = offset;
        memRange.size   = size;
        m_unflushedRanges.emplace_back(memRange);
    }

    void VulkanDevice::flushMappedRanges()
    {
        auto atomSize = m_context->getPhysicalDeviceLimits().nonCoherentAtomSize;
        if (!m_unflushedRanges.empty())
        {
            for (auto& range : m_unflushedRanges)
            {
                if (range.size != VK_WHOLE_SIZE)
                    range.size = ((range.size - 1) / atomSize + 1) * atomSize;

                range.offset = (range.offset / atomSize) * atomSize;
            }

            vkFlushMappedMemoryRanges(m_device, static_cast<uint32_t>(m_unflushedRanges.size()), m_unflushedRanges.data());
            m_unflushedRanges.clear();
        }
    }

    VkSemaphore VulkanDevice::createSemaphore() const
    {
        VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VkSemaphore semaphore;
        vkCreateSemaphore(m_device, &semInfo, nullptr, &semaphore);
        return semaphore;
    }

    VkFence VulkanDevice::createFence(VkFenceCreateFlags flags) const
    {
        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = flags;

        VkFence fence;
        vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
        return fence;
    }

    VulkanMemoryHeap* VulkanDevice::getHeapFromMemProps(VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const
    {
        std::optional<uint32_t> supportedHeapIndex = m_context->findMemoryType(memoryTypeBits, flags);

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
            std::cerr << "Wrong heap type specified when creating buffer!";
            return nullptr;
        }

        if (heap->properties != flags)
        {
            std::cerr << "Buffer has requested unallocated memory type!\n";
            return nullptr;
        }

        return heap;
    }

    VulkanMemoryHeap* VulkanDevice::getDeviceBufferHeap() const
    {
        return m_deviceBufferHeap.get();
    }

    VulkanMemoryHeap* VulkanDevice::getDeviceImageHeap() const
    {
        return m_deviceImageHeap.get();
    }

    VulkanMemoryHeap* VulkanDevice::getStagingBufferHeap() const
    {
        return m_stagingBufferHeap.get();
    }

    void VulkanDevice::postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorBufferInfo bufferInfo)
    {
        m_bufferInfos.emplace_back(bufferInfo);
        m_descriptorWrites.emplace_back(write);
        m_descriptorWrites.back().pBufferInfo = &m_bufferInfos.back();
    }

    void VulkanDevice::postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorImageInfo imageInfo)
    {
        m_imageInfos.emplace_back(imageInfo);
        m_descriptorWrites.emplace_back(write);
        m_descriptorWrites.back().pImageInfo = &m_imageInfos.back();
    }

    void VulkanDevice::flushDescriptorUpdates()
    {
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(m_descriptorWrites.size()), m_descriptorWrites.data(), 0, nullptr);
        m_descriptorWrites.clear();
        m_imageInfos.clear();
        m_bufferInfos.clear();
    }

    void VulkanDevice::printMemoryStatus()
    {
        m_deviceBufferHeap->printFreeChunks();
        m_deviceImageHeap->printFreeChunks();
        m_stagingBufferHeap->printFreeChunks();
    }

    DeviceMemoryMetrics VulkanDevice::getDeviceMemoryUsage()
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
