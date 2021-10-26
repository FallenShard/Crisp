#pragma once

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

namespace crisp
{
    // Retrieve number of channels for a given format
    template <VkFormat... formats> struct NumChannels;
    template <>                    struct NumChannels<>                              { static constexpr uint32_t value = 0; };
    template <>                    struct NumChannels<VK_FORMAT_R32G32B32A32_SFLOAT> { static constexpr uint32_t value = 4; };

    static constexpr uint32_t getNumChannels(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 4;
        default:                            return 0;
        }
    }

    static constexpr uint32_t getSizeOf(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 4 * sizeof(float);
        case VK_FORMAT_R32G32B32_SFLOAT:    return 3 * sizeof(float);
        case VK_FORMAT_R32G32_SFLOAT:       return 2 * sizeof(float);
        default:
            spdlog::critical("Unknown format specified {}", format);
        }

        return ~0u;
    }

    // Retrieve byte size of a vulkan format
    template <VkFormat... formats>        struct FormatSizeof;
    template <>                           struct FormatSizeof<>                              { static constexpr size_t value = 0; };
    template <>                           struct FormatSizeof<VK_FORMAT_R32G32_SFLOAT>       { static constexpr size_t value = 2 * sizeof(float); };
    template <>                           struct FormatSizeof<VK_FORMAT_R32G32B32_SFLOAT>    { static constexpr size_t value = 3 * sizeof(float); };
    template <>                           struct FormatSizeof<VK_FORMAT_R32G32B32A32_SFLOAT> { static constexpr size_t value = 4 * sizeof(float); };
    template <VkFormat F, VkFormat... Fs> struct FormatSizeof<F, Fs...>                      { static constexpr size_t value = FormatSizeof<F>::value + FormatSizeof<Fs...>::value; };

    // Retrieve total byte size for passed types
    template <typename... Ts>             struct AggregateSizeof;
    template <>                           struct AggregateSizeof<>         { static constexpr size_t value = 0; };
    template <typename T>                 struct AggregateSizeof<T>        { static constexpr size_t value = sizeof(T); };
    template <typename T, typename... Ts> struct AggregateSizeof<T, Ts...> { static constexpr size_t value = AggregateSizeof<T>::value + AggregateSizeof<Ts...>::value; };
}