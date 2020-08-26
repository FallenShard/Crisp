#pragma once

#include <vector>
#include <set>
#include <initializer_list>

#include <vulkan/vulkan.h>

#include <CrispCore/BitFlags.hpp>

#include "vulkan/VulkanContext.hpp"

namespace crisp
{
    enum class QueueType
    {
        Graphics = 0x01,
        Compute  = 0x02,
        Transfer = 0x04,
        Present  = 0x08,
        General  = Graphics | Compute | Transfer,
    };
    DECLARE_BITFLAG(QueueType)

    struct QueueIdentifier
    {
        uint32_t familyIndex;
        uint32_t index;
    };

    class VulkanQueueConfiguration
    {
    public:
        VulkanQueueConfiguration(std::initializer_list<QueueTypeFlags> requestedQueuesList);
        VulkanQueueConfiguration(std::initializer_list<QueueTypeFlags> requestedQueuesList, const VulkanContext* context);

        void buildQueueCreateInfos(const VulkanContext* context);
        const std::vector<VkDeviceQueueCreateInfo>& getQueueCreateInfos() const;
        QueueIdentifier getQueueIdentifier(size_t index) const;

    private:
        std::vector<QueueIdentifier> findQueueIds(const VulkanContext* context) const;
        int findQueueFamilyIndex(BitFlags<QueueType> queueType, const VulkanContext* context, const std::vector<VkQueueFamilyProperties>& exposedQueueFamilies, std::vector<uint32_t>& usedQueueFamilyCounts) const;
        QueueTypeFlags getFamilyType(const VulkanContext* context, uint32_t familyIndex, VkQueueFlags queueFlags) const;

        std::vector<QueueTypeFlags> m_requestedQueues;

        std::vector<std::vector<float>> m_queuePriorities;
        std::vector<VkDeviceQueueCreateInfo> m_createInfos;

        std::vector<QueueIdentifier> m_queueIdentifiers;
    };
}