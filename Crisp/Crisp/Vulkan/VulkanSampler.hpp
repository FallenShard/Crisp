#pragma once

#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp {
inline constexpr float MaxAnisotropy = 16.0f;

class VulkanSampler final : public VulkanResource<VkSampler> {
public:
    VulkanSampler(
        const VulkanDevice& device,
        VkFilter minFilter,
        VkFilter magFilter,
        VkSamplerAddressMode addressMode,
        float anisotropy = 1.0f,
        float maxLod = VK_LOD_CLAMP_NONE);
};

std::unique_ptr<VulkanSampler> createLinearClampSampler(
    const VulkanDevice& device, float anisotropy = 1.0f, float maxLod = VK_LOD_CLAMP_NONE);
std::unique_ptr<VulkanSampler> createNearestClampSampler(const VulkanDevice& device);
std::unique_ptr<VulkanSampler> createLinearRepeatSampler(
    const VulkanDevice& device, float anisotropy = 1.0f, float maxLod = VK_LOD_CLAMP_NONE);

} // namespace crisp