#pragma once

#include <vector>
#include <set>
#include <initializer_list>

#include <vulkan/vulkan.h>

#include <CrispCore/BitFlags.hpp>

#include <Crisp/Vulkan/VulkanContext.hpp>

namespace crisp
{
    enum class QueueType
    {
        Graphics           = 0x01,
        Compute            = 0x02,
        Transfer           = 0x04,
        Present            = 0x08,
        AsyncCompute       = Compute | Transfer,
        General            = Graphics | Compute | Transfer,
        GeneralWithPresent = General | Present,
    };
    DECLARE_BITFLAG(QueueType)

    struct QueueIdentifier
    {
        uint32_t familyIndex;
        uint32_t index;
    };

    struct VulkanQueueConfiguration
    {
        std::vector<std::vector<float>> queuePriorities;
        std::vector<VkDeviceQueueCreateInfo> createInfos;
        std::vector<QueueIdentifier> queueIdentifiers;
    };

    VulkanQueueConfiguration createQueueConfiguration(const std::vector<QueueTypeFlags>& requestedQueueTypes, const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice);
    VulkanQueueConfiguration createDefaultQueueConfiguration(const VulkanContext& context, const VulkanPhysicalDevice& physicalDevice);
}