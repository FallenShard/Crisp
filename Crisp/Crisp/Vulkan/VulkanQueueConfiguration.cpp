#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

namespace crisp {
namespace {
QueueTypeFlags getQueueFamilyType(
    const VulkanContext& context,
    const VulkanPhysicalDevice& physicalDevice,
    uint32_t familyIndex,
    VkQueueFlags queueFlags) {
    QueueTypeFlags familyType;

    if (physicalDevice.supportsPresentation(familyIndex, context.getSurface())) {
        familyType |= QueueType::Present;
    }

    if (queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        familyType |= QueueType::Graphics;
    }

    if (queueFlags & VK_QUEUE_COMPUTE_BIT) {
        familyType |= QueueType::Compute;
    }

    if (queueFlags & VK_QUEUE_TRANSFER_BIT) {
        familyType |= QueueType::Transfer;
    }

    return familyType;
}

Result<uint32_t> findQueueFamilyIndex(
    const QueueTypeFlags queueType,
    const VulkanContext& context,
    const VulkanPhysicalDevice& physicalDevice,
    const std::vector<VkQueueFamilyProperties>& exposedQueueFamilies,
    const std::vector<uint32_t>& usedQueueFamilyCounts) {
    // Check if we are requesting an async-compute queue
    if (queueType == QueueType::AsyncCompute) {
        for (uint32_t i = 0; i < exposedQueueFamilies.size(); ++i) {
            const auto& family = exposedQueueFamilies.at(i);
            const auto familyType =
                getQueueFamilyType(context, physicalDevice, static_cast<uint32_t>(i), family.queueFlags);
            if (((familyType & QueueType::AsyncCompute) == QueueType::AsyncCompute) &&
                !(familyType & QueueType::Graphics)) {
                return i;
            }
        }
    }

    // Check for exact family match
    for (uint32_t i = 0; i < exposedQueueFamilies.size(); ++i) {
        const auto& family = exposedQueueFamilies.at(i);
        const auto familyType = getQueueFamilyType(context, physicalDevice, static_cast<uint32_t>(i), family.queueFlags);
        if (familyType == queueType && family.queueCount > usedQueueFamilyCounts[i]) {
            return i;
        }
    }

    // Find a more general family for the queueType
    for (uint32_t i = 0; i < exposedQueueFamilies.size(); ++i) {
        const auto& family = exposedQueueFamilies.at(i);
        const auto familyType = getQueueFamilyType(context, physicalDevice, static_cast<uint32_t>(i), family.queueFlags);
        if ((familyType & queueType) && family.queueCount > usedQueueFamilyCounts[i]) {
            return i;
        }
    }

    return resultError("Failed to find a queue family for queue type! {}", queueType.getMask());
}

Result<std::vector<QueueIdentifier>> findQueueIds(
    const std::vector<QueueTypeFlags>& requestedQueueTypes,
    const VulkanContext& context,
    const VulkanPhysicalDevice& physicalDevice) {
    // Get the queue families that our current physical device has
    const auto exposedQueueFamilies = physicalDevice.queryQueueFamilyProperties();

    // From each family, we can request up to a maximum queue number dependent on the family
    std::vector<uint32_t> usedFamilyCounts(exposedQueueFamilies.size(), 0);

    std::vector<QueueIdentifier> queueIds(requestedQueueTypes.size());

    for (uint32_t i = 0; i < requestedQueueTypes.size(); ++i) {
        const auto familyIndex =
            findQueueFamilyIndex(requestedQueueTypes[i], context, physicalDevice, exposedQueueFamilies, usedFamilyCounts)
                .unwrap();

        queueIds[i].familyIndex = familyIndex;
        queueIds[i].index = usedFamilyCounts[familyIndex]++;
    }

    return queueIds;
}
} // namespace

VulkanQueueConfiguration createQueueConfiguration(
    const std::vector<QueueTypeFlags>& requestedQueueTypes,
    const VulkanContext& context,
    const VulkanPhysicalDevice& physicalDevice) {
    VulkanQueueConfiguration config;
    config.types = requestedQueueTypes;
    config.identifiers = findQueueIds(requestedQueueTypes, context, physicalDevice).unwrap();

    const auto queueFamilies = physicalDevice.queryQueueFamilyProperties();
    std::vector<uint32_t> familyQueueCounts(queueFamilies.size(), 0);
    for (auto& queueId : config.identifiers) {
        familyQueueCounts[queueId.familyIndex]++;
    }

    config.priorities.resize(queueFamilies.size());
    config.createInfos.clear();

    for (uint32_t idx = 0; idx < familyQueueCounts.size(); ++idx) {
        if (familyQueueCounts[idx] != 0) {
            config.priorities[idx].resize(familyQueueCounts[idx], 1.0f);

            VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueCreateInfo.queueFamilyIndex = idx;
            queueCreateInfo.queueCount = familyQueueCounts[idx];
            queueCreateInfo.pQueuePriorities = config.priorities[idx].data();
            config.createInfos.push_back(queueCreateInfo);
        }
    }

    return config;
}

VulkanQueueConfiguration createDefaultQueueConfiguration(
    const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice) {
    return createQueueConfiguration(
        {
            QueueType::GeneralWithPresent,
            QueueType::AsyncCompute,
            QueueType::Transfer,
        },
        context,
        physicalDevice);
}
} // namespace crisp