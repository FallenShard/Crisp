#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
struct VulkanFormatTraits {
    uint32_t channelCount;
    uint32_t byteSize;
};

constexpr VulkanFormatTraits getFormatTraits(const VkFormat format) {
    switch (format) {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return {4, 4 * sizeof(float)};
    case VK_FORMAT_R32G32B32_SFLOAT:
        return {3, 3 * sizeof(float)};
    case VK_FORMAT_R32G32_SFLOAT:
        return {2, 2 * sizeof(float)};
    case VK_FORMAT_R32_SFLOAT:
        return {1, sizeof(float)};
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return {4, 4 * sizeof(uint16_t)};
    case VK_FORMAT_R16G16_SFLOAT:
        return {2, 2 * sizeof(uint16_t)};
    case VK_FORMAT_R16_SFLOAT:
        return {1, sizeof(uint16_t)};
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
        return {4, 4 * sizeof(uint8_t)};
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
        return {3, 3 * sizeof(uint8_t)};
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
        return {2, 2 * sizeof(uint8_t)};
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_UNORM:
        return {1, sizeof(uint8_t)};
    default:
        CRISP_FATAL("Unsupported Vulkan format: {}", static_cast<uint32_t>(format));
    }
}

constexpr uint32_t getChannelCount(const VkFormat format) {
    return getFormatTraits(format).channelCount;
}

constexpr uint32_t getSizeOf(const VkFormat format) {
    return getFormatTraits(format).byteSize;
}

template <VkFormat... Fs>
inline constexpr size_t FormatSizeofValue = (size_t{0} + ... + getSizeOf(Fs));

template <typename... Ts>
inline constexpr size_t AggregateSizeofValue = (size_t{0} + ... + sizeof(Ts));

} // namespace crisp
