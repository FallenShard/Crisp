#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>

#include <stdexcept>

namespace crisp
{
    namespace
    {
        BitFlags<QueueType> getFamilyType(const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice, uint32_t familyIndex, VkQueueFlags queueFlags)
        {
            BitFlags<QueueType> familyType;

            if (physicalDevice.supportsPresentation(familyIndex, context.getSurface()))
                familyType |= QueueType::Present;

            if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
                familyType |= QueueType::Graphics;

            if (queueFlags & VK_QUEUE_COMPUTE_BIT)
                familyType |= QueueType::Compute;

            if (queueFlags & VK_QUEUE_TRANSFER_BIT)
                familyType |= QueueType::Transfer;

            return familyType;
        }

        Result<uint32_t> findQueueFamilyIndex(BitFlags<QueueType> queueType, const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice, const std::vector<VkQueueFamilyProperties>& exposedQueueFamilies, std::vector<uint32_t>& usedQueueFamilyCounts)
        {
            // Check if we are requesting an async-compute queue
            if (queueType == QueueType::AsyncCompute)
            {
                for (uint32_t i = 0; i < exposedQueueFamilies.size(); ++i)
                {
                    const auto& family = exposedQueueFamilies.at(i);
                    const auto familyType = getFamilyType(context, physicalDevice, static_cast<uint32_t>(i), family.queueFlags);
                    if (((familyType & QueueType::AsyncCompute) == QueueType::AsyncCompute) && !(familyType & QueueType::Graphics))
                        return i;
                }
            }

            // Check for exact family match
            for (uint32_t i = 0; i < exposedQueueFamilies.size(); ++i)
            {
                const auto& family = exposedQueueFamilies.at(i);
                const auto familyType = getFamilyType(context, physicalDevice, static_cast<uint32_t>(i), family.queueFlags);
                if (familyType == queueType && family.queueCount > usedQueueFamilyCounts[i])
                    return i;
            }

            // Find a more general family for the queueType
            for (uint32_t i = 0; i < exposedQueueFamilies.size(); ++i)
            {
                const auto& family = exposedQueueFamilies.at(i);
                const auto familyType = getFamilyType(context, physicalDevice, static_cast<uint32_t>(i), family.queueFlags);
                if ((familyType & queueType) && family.queueCount > usedQueueFamilyCounts[i])
                    return i;
            }

            return resultError("Failed to find a queue family for queue type!");
        }
    

        Result<std::vector<QueueIdentifier>> findQueueIds(const std::vector<QueueTypeFlags>& requestedQueueTypes, const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice)
        {
            // Get the queue families that our current physical device has
            const auto exposedQueueFamilies = physicalDevice.queryQueueFamilyProperties();

            // From each family, we can request up to a maximum queue number dependent on the family
            std::vector<uint32_t> usedFamilyCounts(exposedQueueFamilies.size(), 0);

            std::vector<QueueIdentifier> queueIds(requestedQueueTypes.size());

            for (uint32_t i = 0; i < requestedQueueTypes.size(); ++i)
            {
                const auto familyIndex = findQueueFamilyIndex(requestedQueueTypes[i], context, physicalDevice, exposedQueueFamilies, usedFamilyCounts).unwrap();

                queueIds[i].familyIndex = familyIndex;
                queueIds[i].index = usedFamilyCounts[familyIndex]++;
            }

            if (queueIds.size() != requestedQueueTypes.size())
                return resultError("Queue configuration is not compatible with selected physical device!");

            return queueIds;
        }
    }

    VulkanQueueConfiguration createQueueConfiguration(const std::vector<QueueTypeFlags>& requestedQueueTypes, const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice)
    {
        VulkanQueueConfiguration config;
        config.queueIdentifiers = findQueueIds(requestedQueueTypes, context, physicalDevice).unwrap();

        const auto queueFamilies = physicalDevice.queryQueueFamilyProperties();
        std::vector<uint32_t> familyQueueCounts(queueFamilies.size(), 0);
        for (auto& queueId : config.queueIdentifiers)
            familyQueueCounts[queueId.familyIndex]++;

        config.queuePriorities.resize(queueFamilies.size());
        config.createInfos.clear();

        for (size_t idx = 0; idx < familyQueueCounts.size(); ++idx)
        {
            if (familyQueueCounts[idx] != 0)
            {
                config.queuePriorities[idx].resize(familyQueueCounts[idx], 1.0f);

                VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
                queueCreateInfo.queueFamilyIndex = static_cast<uint32_t>(idx);
                queueCreateInfo.queueCount = familyQueueCounts[idx];
                queueCreateInfo.pQueuePriorities = config.queuePriorities[idx].data();
                config.createInfos.push_back(queueCreateInfo);
            }
        }

        return config;
    }

    VulkanQueueConfiguration createDefaultQueueConfiguration(const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice)
    {
        return createQueueConfiguration({
            QueueTypeFlags(QueueType::GeneralWithPresent),
            QueueTypeFlags(QueueType::AsyncCompute),
            QueueTypeFlags(QueueType::Transfer)
        }, context, physicalDevice);
    }
}