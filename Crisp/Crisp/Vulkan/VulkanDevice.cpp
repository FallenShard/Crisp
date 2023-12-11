#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/VulkanChecks.hpp>

namespace crisp {
namespace {
auto logger = createLoggerMt("VulkanDevice");

QueueIdentifier getGeneralQueue(const VulkanQueueConfiguration& queueConfig) {
    return queueConfig.identifiers.at(0);
}

QueueIdentifier getComputeQueue(const VulkanQueueConfiguration& queueConfig) {
    if (queueConfig.identifiers.size() == 1) {
        return queueConfig.identifiers.at(0);
    }

    for (uint32_t i = 0; i < queueConfig.types.size(); ++i) {
        if (queueConfig.types[i] == QueueType::AsyncCompute) {
            return queueConfig.identifiers.at(i);
        }
    }

    return queueConfig.identifiers.at(1);
}

QueueIdentifier getTransferQueue(const VulkanQueueConfiguration& queueConfig) {
    if (queueConfig.identifiers.size() == 1) {
        return queueConfig.identifiers.at(0);
    }

    for (uint32_t i = 0; i < queueConfig.types.size(); ++i) {
        if (queueConfig.types[i] == QueueType::Transfer) {
            return queueConfig.identifiers.at(i);
        }
    }

    return queueConfig.identifiers.at(2);
}

} // namespace

VulkanDevice::VulkanDevice(
    const VulkanPhysicalDevice& physicalDevice, const VulkanQueueConfiguration& queueConfig, int32_t virtualFrameCount)
    : m_handle(createLogicalDeviceHandle(physicalDevice, queueConfig))
    , m_nonCoherentAtomSize(physicalDevice.getLimits().nonCoherentAtomSize)
    , m_debugMarker(std::make_unique<VulkanDebugMarker>(m_handle))
    , m_generalQueue(std::make_unique<VulkanQueue>(m_handle, physicalDevice, ::crisp::getGeneralQueue(queueConfig)))
    , m_computeQueue(std::make_unique<VulkanQueue>(m_handle, physicalDevice, ::crisp::getComputeQueue(queueConfig)))
    , m_transferQueue(std::make_unique<VulkanQueue>(m_handle, physicalDevice, ::crisp::getTransferQueue(queueConfig)))
    , m_memoryAllocator(std::make_unique<VulkanMemoryAllocator>(physicalDevice, m_handle))
    , m_resourceDeallocator(std::make_unique<VulkanResourceDeallocator>(m_handle, virtualFrameCount)) {
    m_debugMarker->setObjectName(m_generalQueue->getHandle(), "General Queue");
    m_debugMarker->setObjectName(m_computeQueue->getHandle(), "Compute Queue");
    m_debugMarker->setObjectName(m_transferQueue->getHandle(), "Transfer Queue");
}

VulkanDevice::~VulkanDevice() {
    m_resourceDeallocator->freeAllResources();
    m_memoryAllocator.reset();
    vkDestroyDevice(m_handle, nullptr);
}

VkDevice VulkanDevice::getHandle() const {
    return m_handle;
}

const VulkanQueue& VulkanDevice::getGeneralQueue() const {
    return *m_generalQueue;
}

const VulkanQueue& VulkanDevice::getComputeQueue() const {
    return *m_computeQueue;
}

const VulkanQueue& VulkanDevice::getTransferQueue() const {
    return *m_transferQueue;
}

void VulkanDevice::invalidateMappedRange(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange memRange = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    memRange.memory = memory;
    memRange.offset = offset;
    memRange.size = size;

    /*auto atomSize = m_context->getPhysicalDeviceLimits().nonCoherentAtomSize;
    auto old = offset;
    offset = (offset / atomSize) * atomSize;
    if (old != offset)
        std::cout << "Old: " << old << " New: " << offset << std::endl;*/

    m_unflushedRanges.emplace_back(memRange);
}

void VulkanDevice::flushMappedRanges() {
    if (!m_unflushedRanges.empty()) {
        for (auto& range : m_unflushedRanges) {
            if (range.size != VK_WHOLE_SIZE) {
                range.size = ((range.size - 1) / m_nonCoherentAtomSize + 1) * m_nonCoherentAtomSize;
            }

            range.offset = (range.offset / m_nonCoherentAtomSize) * m_nonCoherentAtomSize;
        }

        vkFlushMappedMemoryRanges(m_handle, static_cast<uint32_t>(m_unflushedRanges.size()), m_unflushedRanges.data());
        m_unflushedRanges.clear();
    }
}

VkSemaphore VulkanDevice::createSemaphore() const {
    VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkSemaphore semaphore{VK_NULL_HANDLE};
    vkCreateSemaphore(m_handle, &semInfo, nullptr, &semaphore);
    return semaphore;
}

VkFence VulkanDevice::createFence(VkFenceCreateFlags flags) const {
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = flags;

    VkFence fence{VK_NULL_HANDLE};
    vkCreateFence(m_handle, &fenceInfo, nullptr, &fence);
    return fence;
}

VkBuffer VulkanDevice::createBuffer(const VkBufferCreateInfo& bufferCreateInfo) const {
    VkBuffer buffer{VK_NULL_HANDLE};
    vkCreateBuffer(m_handle, &bufferCreateInfo, nullptr, &buffer);
    return buffer;
}

VkImage VulkanDevice::createImage(const VkImageCreateInfo& imageCreateInfo) const {
    VkImage image{VK_NULL_HANDLE};
    vkCreateImage(m_handle, &imageCreateInfo, nullptr, &image);
    return image;
}

void VulkanDevice::postDescriptorWrite(const VkWriteDescriptorSet& write, const VkDescriptorBufferInfo& bufferInfo) {
    m_bufferInfos.push_back({bufferInfo});
    m_descriptorWrites.emplace_back(write);
    m_descriptorWrites.back().pBufferInfo = m_bufferInfos.back().data();
}

void VulkanDevice::postDescriptorWrite(
    const VkWriteDescriptorSet& write, std::vector<VkDescriptorBufferInfo>&& bufferInfos) {
    m_bufferInfos.emplace_back(std::move(bufferInfos));
    m_descriptorWrites.emplace_back(write);
    m_descriptorWrites.back().pBufferInfo = m_bufferInfos.back().data();
}

void VulkanDevice::postDescriptorWrite(const VkWriteDescriptorSet& write, const VkDescriptorImageInfo& imageInfo) {
    m_imageInfos.emplace_back(imageInfo);
    m_descriptorWrites.emplace_back(write);
    m_descriptorWrites.back().pImageInfo = &m_imageInfos.back();
}

void VulkanDevice::postDescriptorWrite(const VkWriteDescriptorSet& write) {
    m_descriptorWrites.emplace_back(write);
}

void VulkanDevice::flushDescriptorUpdates() {
    if (!m_descriptorWrites.empty()) {
        vkUpdateDescriptorSets(
            m_handle, static_cast<uint32_t>(m_descriptorWrites.size()), m_descriptorWrites.data(), 0, nullptr);
    }

    m_descriptorWrites.clear();
    m_imageInfos.clear();
    m_bufferInfos.clear();
}

VkDevice createLogicalDeviceHandle(const VulkanPhysicalDevice& physicalDevice, const VulkanQueueConfiguration& config) {
    std::vector<const char*> enabledExtensions;
    std::ranges::transform(
        physicalDevice.getDeviceExtensions(), std::back_inserter(enabledExtensions), [](const std::string& ext) {
            return ext.c_str();
        });

    VkDeviceCreateInfo createInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.pNext = &physicalDevice.getFeatures2();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(config.createInfos.size());
    createInfo.pQueueCreateInfos = config.createInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VkDevice device(VK_NULL_HANDLE);
    VK_CHECK(vkCreateDevice(physicalDevice.getHandle(), &createInfo, nullptr, &device));
    loadVulkanDeviceFunctions(device);
    return device;
}
} // namespace crisp
