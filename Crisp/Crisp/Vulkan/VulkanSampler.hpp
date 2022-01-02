#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
class VulkanDevice;

class VulkanSampler final : public VulkanResource<VkSampler, vkDestroySampler>
{
public:
    VulkanSampler(const VulkanDevice& device, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode);
    VulkanSampler(const VulkanDevice& device, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode,
        float anisotropy, float maxLod);
};

std::unique_ptr<VulkanSampler> createLinearClampSampler(const VulkanDevice& device);

} // namespace crisp