#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

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

VmaAllocator createMemoryAllocator(
    const VulkanPhysicalDevice& physicalDevice, const VkDevice deviceHandle, const VulkanInstance& instance) {
    VmaVulkanFunctions functions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR = vkBindBufferMemory2,
        .vkBindImageMemory2KHR = vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
        .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
        .vkGetMemoryWin32HandleKHR = vkGetMemoryWin32HandleKHR,
    };

    VmaAllocatorCreateInfo createInfo{
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physicalDevice.getHandle(),
        .device = deviceHandle,
        .pVulkanFunctions = &functions,
        .instance = instance.getHandle(),
        .vulkanApiVersion = instance.getApiVersion(),
    };

    VmaAllocator allocator{};
    vmaCreateAllocator(&createInfo, &allocator);
    return allocator;
}

} // namespace

VulkanDevice::VulkanDevice(
    const VulkanPhysicalDevice& physicalDevice,
    const VulkanQueueConfiguration& queueConfig,
    const VulkanInstance& instance,
    const int32_t /*virtualFrameCount*/)
    : m_handle(createLogicalDeviceHandle(physicalDevice, queueConfig))
    , m_nonCoherentAtomSize(physicalDevice.getLimits().nonCoherentAtomSize)
    , m_generalQueue(std::make_unique<VulkanQueue>(m_handle, physicalDevice, ::crisp::getGeneralQueue(queueConfig)))
    , m_computeQueue(std::make_unique<VulkanQueue>(m_handle, physicalDevice, ::crisp::getComputeQueue(queueConfig)))
    , m_transferQueue(std::make_unique<VulkanQueue>(m_handle, physicalDevice, ::crisp::getTransferQueue(queueConfig)))
    , m_memoryAllocator(createMemoryAllocator(physicalDevice, m_handle, instance))
    , m_resourceDeallocator(std::make_unique<VulkanResourceDeallocator>(m_handle, m_memoryAllocator))
    , m_debugUtilsEnabled(instance.isExtensionEnabled(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    setObjectName(m_generalQueue->getHandle(), "General Queue");
    setObjectName(m_computeQueue->getHandle(), "Compute Queue");
    setObjectName(m_transferQueue->getHandle(), "Transfer Queue");
}

VulkanDevice::~VulkanDevice() {
    m_resourceDeallocator->freeAllResources();
    vmaDestroyAllocator(m_memoryAllocator);
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

        m_descriptorWrites.clear();
        m_imageInfos.clear();
        m_bufferInfos.clear();
    }
}

void VulkanDevice::waitIdle() const {
    vkDeviceWaitIdle(m_handle);
}

void VulkanDevice::postResourceUpdate(const std::function<void(VkCommandBuffer)>& resourceUpdateFunc) {
    m_resourceUpdates.emplace_back(resourceUpdateFunc);
}

void VulkanDevice::executeResourceUpdates(const VkCommandBuffer cmdBuffer) {
    if (m_resourceUpdates.empty()) {
        return;
    }

    for (const auto& update : m_resourceUpdates) {
        update(cmdBuffer);
    }
    m_resourceUpdates.clear();
}

void VulkanDevice::flushResourceUpdates(const bool waitOnAllQueues) {
    if (m_resourceUpdates.empty()) {
        return;
    }

    getGeneralQueue().submitAndWait([this](const VkCommandBuffer cmdBuffer) { executeResourceUpdates(cmdBuffer); });

    if (waitOnAllQueues) {
        waitIdle();
    }
}

void VulkanDevice::setObjectName(const uint64_t vulkanHandle, const char* name, const VkObjectType objectType) const {
    if (!m_debugUtilsEnabled) {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = vulkanHandle;
    nameInfo.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(m_handle, &nameInfo);
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
