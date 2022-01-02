#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
struct VulkanBufferSpan
{
    VkBuffer handle;
    VkDeviceSize offset;
    VkDeviceSize size;
};
} // namespace crisp
