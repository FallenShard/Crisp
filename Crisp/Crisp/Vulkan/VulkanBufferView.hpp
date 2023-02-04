#pragma once

#include <Crisp/Vulkan/VulkanHeader.hpp>

namespace crisp
{
struct VulkanBufferSpan
{
    VkBuffer handle;
    VkDeviceSize offset;
    VkDeviceSize size;
};
} // namespace crisp
