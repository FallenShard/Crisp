#pragma once

#include <vulkan/vulkan.h>

namespace crisp
{
    template<VkFormat format>
    struct NumChannels
    {
        static constexpr uint32_t value = 0;
    };

    template<> struct NumChannels<VK_FORMAT_R32G32B32A32_SFLOAT> { static constexpr uint32_t value = 4; };

    uint32_t getNumChannels(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 4;
        default:                            return 0;
        }
    }
}