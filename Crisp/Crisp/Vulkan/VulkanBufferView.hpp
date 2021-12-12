#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    struct VulkanBufferView
    {
        VkBuffer handle;
        VkDeviceSize offset;
        VkDeviceSize size;
    };
}
