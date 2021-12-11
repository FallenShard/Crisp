#include "VulkanSampler.hpp"

#include "VulkanDevice.hpp"

namespace crisp
{
    VulkanSampler::VulkanSampler(VulkanDevice* device, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode)
        : VulkanSampler(device, minFilter, magFilter, addressMode, 1.0f, 0.0f)
    {
    }

    VulkanSampler::VulkanSampler(VulkanDevice* device, VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode, float anisotropy, float maxLod)
        : VulkanResource(device->getResourceDeallocator())
    {
        VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter               = magFilter;
        samplerInfo.minFilter               = minFilter;
        samplerInfo.addressModeU            = addressMode;
        samplerInfo.addressModeV            = addressMode;
        samplerInfo.addressModeW            = addressMode;
        samplerInfo.anisotropyEnable        = anisotropy == 1.0f ? VK_FALSE : VK_TRUE;
        samplerInfo.maxAnisotropy           = anisotropy;
        samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = maxLod;
        vkCreateSampler(device->getHandle(), &samplerInfo, nullptr, &m_handle);
    }
}