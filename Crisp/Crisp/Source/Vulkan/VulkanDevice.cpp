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
        VulkanQueueConfiguration config =
        {
            QueueType::General | QueueType::Present,
            QueueType::Transfer
        };
        config.buildQueueCreateInfos(vulkanContext);
        m_device = m_context->createLogicalDevice(config); 

        m_generalQueue  = std::make_unique<VulkanQueue>(this, config.getQueueIdentifier(0));
        m_transferQueue = std::make_unique<VulkanQueue>(this, config.getQueueIdentifier(1));

        // Device buffer memory
        m_deviceBufferHeap = std::make_unique<MemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DeviceHeapSize, m_context->findDeviceBufferMemoryType(m_device), m_device, "Device Buffer Heap");

        // Device image memory
        m_deviceImageHeap = std::make_unique<MemoryHeap>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 2 * DeviceHeapSize, m_context->findDeviceImageMemoryType(m_device), m_device, "Device Image Heap");

        // Staging memory
        m_stagingBufferHeap = std::make_unique<MemoryHeap>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingHeapSize, m_context->findStagingBufferMemoryType(m_device), m_device, "Staging Buffer Heap");
        vkMapMemory(m_device, m_stagingBufferHeap->memory, 0, m_stagingBufferHeap->size, 0, &m_mappedStagingPtr);
    }

    VulkanDevice::~VulkanDevice()
    {
        vkUnmapMemory(m_device, m_stagingBufferHeap->memory);
        
        vkFreeMemory(m_device, m_deviceBufferHeap->memory, nullptr);
        vkFreeMemory(m_device, m_deviceImageHeap->memory, nullptr);
        vkFreeMemory(m_device, m_stagingBufferHeap->memory, nullptr);

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
        VkMappedMemoryRange memRange = {};
        memRange.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memRange.memory = memory;
        memRange.offset = offset;
        memRange.size   = size;
        m_unflushedRanges.emplace_back(memRange);
    }

    void VulkanDevice::flushMappedRanges()
    {
        if (!m_unflushedRanges.empty())
        {
            vkFlushMappedMemoryRanges(m_device, static_cast<uint32_t>(m_unflushedRanges.size()), m_unflushedRanges.data());
            m_unflushedRanges.clear();
        }
    }

    VkSampler VulkanDevice::createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode)
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = magFilter;
        samplerInfo.minFilter               = minFilter;
        samplerInfo.addressModeU            = addressMode;
        samplerInfo.addressModeV            = addressMode;
        samplerInfo.addressModeW            = addressMode;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 0.0f;

        VkSampler sampler = VK_NULL_HANDLE;
        vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler);
        return sampler;
    }

    VkSemaphore VulkanDevice::createSemaphore() const
    {
        VkSemaphoreCreateInfo semInfo = {};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        vkCreateSemaphore(m_device, &semInfo, nullptr, &semaphore);
        return semaphore;
    }

    VkFence VulkanDevice::createFence(VkFenceCreateFlags flags) const
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;
        fenceInfo.flags = flags;

        VkFence fence;
        vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
        return fence;
    }

    MemoryHeap* VulkanDevice::getHeapFromMemProps(VkBuffer buffer, VkMemoryPropertyFlags flags, uint32_t memoryTypeBits) const
    {
        uint32_t supportedHeapIndex = m_context->findMemoryType(memoryTypeBits, flags);

        MemoryHeap* heap = nullptr;
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

    MemoryHeap* VulkanDevice::getDeviceBufferHeap() const
    {
        return m_deviceBufferHeap.get();
    }

    MemoryHeap* VulkanDevice::getDeviceImageHeap() const
    {
        return m_deviceImageHeap.get();
    }

    MemoryHeap* VulkanDevice::getStagingBufferHeap() const
    {
        return m_stagingBufferHeap.get();
    }

    void* VulkanDevice::getStagingMemoryPtr() const
    {
        return m_mappedStagingPtr;
    }

    void VulkanDevice::printMemoryStatus()
    {
        auto printSingleHeap = [](MemoryHeap* heap)
        {
            std::cout << heap->tag << "\n";
            std::cout << "  Free chunks: \n";
            for (auto& chunk : heap->freeChunks)
                std::cout << "    " << chunk.first << " - " << chunk.second << "-" << chunk.first + chunk.second << "\n";
        };

        printSingleHeap(m_deviceBufferHeap.get());
        printSingleHeap(m_deviceImageHeap.get());
        printSingleHeap(m_stagingBufferHeap.get());
    }

    DeviceMemoryMetrics VulkanDevice::getDeviceMemoryUsage()
    {
        DeviceMemoryMetrics memoryMetrics = {};
        memoryMetrics.bufferMemorySize  = m_deviceBufferHeap->size;
        memoryMetrics.bufferMemoryUsed  = m_deviceBufferHeap->usedSize;
        memoryMetrics.imageMemorySize   = m_deviceImageHeap->size;
        memoryMetrics.imageMemoryUsed   = m_deviceImageHeap->usedSize;
        memoryMetrics.stagingMemorySize = m_stagingBufferHeap->size;
        memoryMetrics.stagingMemoryUsed = m_stagingBufferHeap->usedSize;
        return memoryMetrics;
    }
}
