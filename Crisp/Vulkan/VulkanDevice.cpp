#include "VulkanDevice.hpp"

#include <iostream>

#include "VulkanContext.hpp"
#include "VulkanQueueConfiguration.hpp"
#include "VulkanQueue.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace
{
    auto logger = spdlog::stderr_color_st("VulkanDevice");
}

namespace crisp
{
    VulkanDevice::VulkanDevice(VulkanContext* vulkanContext, int virtualFrameCount)
        : m_context(vulkanContext)
        , m_currentFrameIndex(0)
        , m_virtualFrameCount(virtualFrameCount)
        , m_device(VK_NULL_HANDLE)
    {
        const VulkanQueueConfiguration config({
            QueueTypeFlags(QueueType::General | QueueType::Present),
            QueueTypeFlags(QueueType::Compute),
            QueueTypeFlags(QueueType::Transfer)
        }, m_context);
        m_device = m_context->createLogicalDevice(config);

        m_generalQueue = std::make_unique<VulkanQueue>(this, config.getQueueIdentifier(0));
        m_computeQueue = std::make_unique<VulkanQueue>(this, config.getQueueIdentifier(1));
        m_transferQueue = std::make_unique<VulkanQueue>(this, config.getQueueIdentifier(2));

        m_memoryAllocator = std::make_unique<VulkanMemoryAllocator>(*m_context, m_device);
    }

    VulkanDevice::~VulkanDevice()
    {
        for (auto& destructor : m_deferredDestructors)
            destructor.destructorCallback(destructor.vulkanHandle, this);

        for (auto& memoryChunkPair : m_deferredMemoryChunks)
            memoryChunkPair.second.free();

        m_memoryAllocator.reset();
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

        /*auto atomSize = m_context->getPhysicalDeviceLimits().nonCoherentAtomSize;
        auto old = offset;
        offset = (offset / atomSize) * atomSize;
        if (old != offset)
            std::cout << "Old: " << old << " New: " << offset << std::endl;*/

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

    void VulkanDevice::updateDeferredDestructions()
    {
        // Handle deferred destructors
        for (auto& destructor : m_deferredDestructors)
        {
            if (--destructor.framesRemaining < 0)
            {
                destructor.destructorCallback(destructor.vulkanHandle, this);
            }
        }

        m_deferredDestructors.erase(std::remove_if(
            m_deferredDestructors.begin(),
            m_deferredDestructors.end(), [](const auto& a) {
                return a.framesRemaining < 0;
            }), m_deferredDestructors.end());


        // Free the memory chunks
        for (auto& memoryChunkPair : m_deferredMemoryChunks)
        {
            if (--memoryChunkPair.first < 0)
                memoryChunkPair.second.free();
        }

        m_deferredMemoryChunks.erase(std::remove_if(
            m_deferredMemoryChunks.begin(),
            m_deferredMemoryChunks.end(), [](const auto& a) {
                return a.first < 0;
            }), m_deferredMemoryChunks.end());
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

    void VulkanDevice::postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorBufferInfo bufferInfo)
    {
        m_bufferInfos.emplace_back(std::vector<VkDescriptorBufferInfo>(1, bufferInfo));
        m_descriptorWrites.emplace_back(write);
        m_descriptorWrites.back().pBufferInfo = m_bufferInfos.back().data();
    }

    void VulkanDevice::postDescriptorWrite(VkWriteDescriptorSet&& write, std::vector<VkDescriptorBufferInfo>&& bufferInfos)
    {
        m_bufferInfos.emplace_back(std::move(bufferInfos));
        m_descriptorWrites.emplace_back(write);
        m_descriptorWrites.back().pBufferInfo = m_bufferInfos.back().data();
    }

    void VulkanDevice::postDescriptorWrite(VkWriteDescriptorSet&& write, VkDescriptorImageInfo imageInfo)
    {
        m_imageInfos.emplace_back(imageInfo);
        m_descriptorWrites.emplace_back(write);
        m_descriptorWrites.back().pImageInfo = &m_imageInfos.back();
    }

    void VulkanDevice::flushDescriptorUpdates()
    {
        if (!m_descriptorWrites.empty())
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(m_descriptorWrites.size()), m_descriptorWrites.data(), 0, nullptr);

        m_descriptorWrites.clear();
        m_imageInfos.clear();
        m_bufferInfos.clear();
    }

    void VulkanDevice::setCurrentFrameIndex(uint64_t frameIndex)
    {
        m_currentFrameIndex = frameIndex;
    }
}
