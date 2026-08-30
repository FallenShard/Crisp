#pragma once

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {
inline constexpr float MaxAnisotropy = 16.0f;

VkSamplerCreateInfo createLinearClampSamplerCreateInfo(
    float anisotropy = 1.0f, float maxLod = VK_LOD_CLAMP_NONE);
VkSamplerCreateInfo createNearestClampSamplerCreateInfo();
VkSamplerCreateInfo createLinearRepeatSamplerCreateInfo(
    float anisotropy = 1.0f, float maxLod = VK_LOD_CLAMP_NONE);
VkSamplerCreateInfo createLatLongEnvironmentSamplerCreateInfo();

class VulkanSampler final : public VulkanResource<VkSampler> {
public:
    VulkanSampler(const VulkanDevice& device, const VkSamplerCreateInfo& createInfo);
};

std::unique_ptr<VulkanSampler> createLinearClampSampler(
    const VulkanDevice& device, float anisotropy = 1.0f, float maxLod = VK_LOD_CLAMP_NONE);
std::unique_ptr<VulkanSampler> createNearestClampSampler(const VulkanDevice& device);
std::unique_ptr<VulkanSampler> createLinearRepeatSampler(
    const VulkanDevice& device, float anisotropy = 1.0f, float maxLod = VK_LOD_CLAMP_NONE);

} // namespace crisp
