#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>

namespace crisp
{
    class VulkanDevice;

    class VulkanSampler : public VulkanResource<VkSampler, vkDestroySampler>
    {
    public:
        VulkanSampler(VulkanDevice& device, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode);
        VulkanSampler(VulkanDevice& device, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode, float anisotropy, float maxLod);
    };
}