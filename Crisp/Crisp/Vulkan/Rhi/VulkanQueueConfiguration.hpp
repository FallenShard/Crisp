#pragma once

#include <Crisp/Utils/BitFlags.hpp>
#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>

namespace crisp {
enum class QueueType : uint8_t {
    Graphics = 0x01,
    Compute = 0x02,
    Transfer = 0x04,
    Present = 0x08,
    AsyncCompute = Compute | Transfer,
    General = Graphics | Compute | Transfer,
    GeneralWithPresent = General | Present,
};
DECLARE_BITFLAG(QueueType)

struct QueueIdentifier {
    uint32_t familyIndex;
    uint32_t index;
};

struct VulkanQueueConfiguration {
    std::vector<std::vector<float>> priorities;
    std::vector<VkDeviceQueueCreateInfo> createInfos;
    std::vector<QueueIdentifier> identifiers;
    std::vector<BitFlags<QueueType>> types;
};

VulkanQueueConfiguration createQueueConfiguration(
    const std::vector<QueueTypeFlags>& requestedQueueTypes,
    const VulkanInstance& instance,
    const VulkanPhysicalDevice& physicalDevice);
VulkanQueueConfiguration createDefaultQueueConfiguration(
    const VulkanInstance& instance, const VulkanPhysicalDevice& physicalDevice);
} // namespace crisp