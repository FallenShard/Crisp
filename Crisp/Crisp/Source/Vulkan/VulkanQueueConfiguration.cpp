#include "VulkanQueueConfiguration.hpp"

namespace crisp
{
    VulkanQueueConfiguration::VulkanQueueConfiguration(std::initializer_list<BitFlags<QueueType>> requestedQueuesList)
        : m_requestedQueues(requestedQueuesList)
    {
    }

    void VulkanQueueConfiguration::buildQueueCreateInfos(const VulkanContext* context)
    {
        m_queueIdentifiers = findQueueIds(context);
        if (m_queueIdentifiers.size() != m_requestedQueues.size())
            throw std::exception("Queue configuration is not compatible with selected physical device!");

        auto queueFamilies = context->getQueueFamilyProperties();

        std::vector<uint32_t> familyQueueCounts(queueFamilies.size(), 0);
        for (auto& queueId : m_queueIdentifiers)
            familyQueueCounts[queueId.familyIndex]++;

        m_queuePriorities.resize(queueFamilies.size());
        m_createInfos.clear();

        for (size_t idx = 0; idx < familyQueueCounts.size(); ++idx)
        {
            if (familyQueueCounts[idx] != 0)
            {
                m_queuePriorities[idx].resize(familyQueueCounts[idx], 1.0f);

                VkDeviceQueueCreateInfo queueCreateInfo = {};
                queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = static_cast<uint32_t>(idx);
                queueCreateInfo.queueCount       = familyQueueCounts[idx];
                queueCreateInfo.pQueuePriorities = m_queuePriorities[idx].data();
                m_createInfos.push_back(queueCreateInfo);
            }
        }
    }

    const std::vector<VkDeviceQueueCreateInfo>& VulkanQueueConfiguration::getQueueCreateInfos() const
    {
        return m_createInfos;
    }

    QueueIdentifier VulkanQueueConfiguration::getQueueIdentifier(size_t index) const
    {
        return m_queueIdentifiers.at(index);
    }

    std::vector<QueueIdentifier> VulkanQueueConfiguration::findQueueIds(const VulkanContext* context) const
    {
        auto exposedQueueFamilies = context->getQueueFamilyProperties();
        std::vector<uint32_t> usedFamilyCounts(exposedQueueFamilies.size(), 0);

        std::vector<QueueIdentifier> queueIds(m_requestedQueues.size());

        size_t idx = 0;
        for (const auto& queue : m_requestedQueues)
        {
            auto familyIndex = findQueueFamilyIndex(queue, context, exposedQueueFamilies, usedFamilyCounts);
            if (familyIndex == -1)
                continue;

            queueIds[idx].familyIndex = familyIndex;
            queueIds[idx].index = usedFamilyCounts[static_cast<size_t>(familyIndex)];

            usedFamilyCounts[static_cast<size_t>(familyIndex)]++;

            idx++;
        }

        return queueIds;
    }

    int VulkanQueueConfiguration::findQueueFamilyIndex(BitFlags<QueueType> queueType, const VulkanContext* context, const std::vector<VkQueueFamilyProperties>& exposedQueueFamilies, std::vector<uint32_t>& usedQueueFamilyCounts) const
    {
        // Check for exact family match first
        int i = 0;
        for (const auto& family : exposedQueueFamilies)
        {
            auto familyType = getFamilyType(context, static_cast<uint32_t>(i), family.queueFlags);
            if (familyType == queueType && family.queueCount > usedQueueFamilyCounts[i])
                return i;

            i++;
        }

        // Find a more general family for the queueType
        i = 0;
        for (const auto& family : exposedQueueFamilies)
        {
            auto familyType = getFamilyType(context, static_cast<uint32_t>(i), family.queueFlags);
            if ((familyType & queueType) && family.queueCount > usedQueueFamilyCounts[i])
                return i;

            i++;
        }

        return -1;
    }

    BitFlags<QueueType> VulkanQueueConfiguration::getFamilyType(const VulkanContext* context, uint32_t familyIndex, VkQueueFlags queueFlags) const
    {
        BitFlags<QueueType> familyType;

        if (context->getQueueFamilyPresentationSupport(familyIndex))
            familyType |= QueueType::Present;

        if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
            familyType |= QueueType::Graphics;

        if (queueFlags & VK_QUEUE_COMPUTE_BIT)
            familyType |= QueueType::Compute;

        if (queueFlags & VK_QUEUE_TRANSFER_BIT)
            familyType |= QueueType::Transfer;

        return familyType;
    }
}