#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>

#include <CrispCore/Logger.hpp>

namespace crisp
{
    namespace
    {
        auto logger = createLoggerMt("VulkanDevice");
    }

    VulkanDevice::VulkanDevice(const VulkanPhysicalDevice& physicalDevice, const VulkanQueueConfiguration& queueConfig, int32_t virtualFrameCount)
        : m_handle(physicalDevice.createLogicalDevice(queueConfig))
        , m_physicalDevice(&physicalDevice)
        , m_generalQueue(std::make_unique<VulkanQueue>(*this, queueConfig.queueIdentifiers.at(0)))
        , m_computeQueue(std::make_unique<VulkanQueue>(*this, queueConfig.queueIdentifiers.at(1)))
        , m_transferQueue(std::make_unique<VulkanQueue>(*this, queueConfig.queueIdentifiers.at(2)))
        , m_memoryAllocator(std::make_unique<VulkanMemoryAllocator>(physicalDevice, m_handle))
        , m_resourceDeallocator(std::make_unique<VulkanResourceDeallocator>(m_handle, virtualFrameCount))
    {
    }

    VulkanDevice::~VulkanDevice()
    {
        m_resourceDeallocator->freeAllResources();
        m_memoryAllocator.reset();
        vkDestroyDevice(m_handle, nullptr);
    }

    VkDevice VulkanDevice::getHandle() const
    {
        return m_handle;
    }

    const VulkanPhysicalDevice& VulkanDevice::getPhysicalDevice() const
    {
        return *m_physicalDevice;
    }

    const VulkanQueue& VulkanDevice::getGeneralQueue() const
    {
        return *m_generalQueue;
    }

    const VulkanQueue& VulkanDevice::getComputeQueue() const
    {
        return *m_computeQueue;
    }

    const VulkanQueue& VulkanDevice::getTransferQueue() const
    {
        return *m_transferQueue;
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
        const VkDeviceSize nonCoherentAtomSize = m_physicalDevice->getLimits().nonCoherentAtomSize;
        if (!m_unflushedRanges.empty())
        {
            for (auto& range : m_unflushedRanges)
            {
                if (range.size != VK_WHOLE_SIZE)
                    range.size = ((range.size - 1) / nonCoherentAtomSize + 1) * nonCoherentAtomSize;

                range.offset = (range.offset / nonCoherentAtomSize) * nonCoherentAtomSize;
            }

            vkFlushMappedMemoryRanges(m_handle, static_cast<uint32_t>(m_unflushedRanges.size()), m_unflushedRanges.data());
            m_unflushedRanges.clear();
        }
    }

    VkSemaphore VulkanDevice::createSemaphore() const
    {
        VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VkSemaphore semaphore;
        vkCreateSemaphore(m_handle, &semInfo, nullptr, &semaphore);
        return semaphore;
    }

    VkFence VulkanDevice::createFence(VkFenceCreateFlags flags) const
    {
        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = flags;

        VkFence fence;
        vkCreateFence(m_handle, &fenceInfo, nullptr, &fence);
        return fence;
    }

    VkBuffer VulkanDevice::createBuffer(const VkBufferCreateInfo& bufferCreateInfo) const
    {
        VkBuffer bufferHandle;
        vkCreateBuffer(m_handle, &bufferCreateInfo, nullptr, &bufferHandle);
        return bufferHandle;
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
            vkUpdateDescriptorSets(m_handle, static_cast<uint32_t>(m_descriptorWrites.size()), m_descriptorWrites.data(), 0, nullptr);

        m_descriptorWrites.clear();
        m_imageInfos.clear();
        m_bufferInfos.clear();
    }
}
