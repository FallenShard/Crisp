#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>

namespace crisp {
// Retrieve number of channels for a given format
template <VkFormat... formats>
struct NumChannels;

template <>
struct NumChannels<> {
    static constexpr uint32_t value = 0;
};

template <>
struct NumChannels<VK_FORMAT_R32G32B32A32_SFLOAT> {
    static constexpr uint32_t value = 4;
};

inline consteval uint32_t getNumChannels(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
        return 4;
    case VK_FORMAT_R8_UNORM:
        return 1;
    default:
        CRISP_FATAL("Could not determine number of channels for format: {}", static_cast<uint32_t>(format));
    }
}

inline constexpr uint32_t getSizeOf(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 4 * sizeof(float);
    case VK_FORMAT_R32G32B32_SFLOAT:
        return 3 * sizeof(float);
    case VK_FORMAT_R32G32_SFLOAT:
        return 2 * sizeof(float);
    default:
        CRISP_FATAL("Could not determine byte size for format: {}", static_cast<uint32_t>(format));
    }
}

// Retrieve byte size of a vulkan format
template <VkFormat... formats>
struct FormatSizeof;

template <>
struct FormatSizeof<> {
    static constexpr size_t value = 0;
};

template <>
struct FormatSizeof<VK_FORMAT_R32G32_SFLOAT> {
    static constexpr size_t value = 2 * sizeof(float);
};

template <>
struct FormatSizeof<VK_FORMAT_R32G32B32_SFLOAT> {
    static constexpr size_t value = 3 * sizeof(float);
};

template <>
struct FormatSizeof<VK_FORMAT_R32G32B32A32_SFLOAT> {
    static constexpr size_t value = 4 * sizeof(float);
};

template <VkFormat F, VkFormat... Fs>
struct FormatSizeof<F, Fs...> {
    static constexpr size_t value = FormatSizeof<F>::value + FormatSizeof<Fs...>::value;
};

template <VkFormat... Fs>
inline constexpr size_t FormatSizeofValue = FormatSizeof<Fs...>::value;

// Retrieve total byte size for passed types
template <typename... Ts>
struct AggregateSizeof;

template <>
struct AggregateSizeof<> {
    static constexpr size_t value = 0;
};

template <typename T>
struct AggregateSizeof<T> {
    static constexpr size_t value = sizeof(T);
};

template <typename T, typename... Ts>
struct AggregateSizeof<T, Ts...> {
    static constexpr size_t value = AggregateSizeof<T>::value + AggregateSizeof<Ts...>::value;
};
} // namespace crisp