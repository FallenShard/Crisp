#include <Crisp/Vulkan/VulkanSampler.hpp>

namespace crisp
{
VulkanSampler::VulkanSampler(
    const VulkanDevice& device,
    const VkFilter minFilter,
    const VkFilter magFilter,
    const VkSamplerAddressMode addressMode,
    const float anisotropy,
    const float maxLod)
    : VulkanResource(device.getResourceDeallocator())
{
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = anisotropy == 1.0f ? VK_FALSE : VK_TRUE;
    samplerInfo.maxAnisotropy = anisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = maxLod;
    vkCreateSampler(device.getHandle(), &samplerInfo, nullptr, &m_handle);
}

std::unique_ptr<VulkanSampler> createLinearClampSampler(
    const VulkanDevice& device, const float anisotropy, const float maxLod)
{
    auto sampler = std::make_unique<VulkanSampler>(
        device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, anisotropy, maxLod);
    device.getDebugMarker().setObjectName(*sampler, "Linear Clamp Sampler");
    return sampler;
}

std::unique_ptr<VulkanSampler> createNearestClampSampler(const VulkanDevice& device)
{
    auto sampler = std::make_unique<VulkanSampler>(
        device, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    device.getDebugMarker().setObjectName(*sampler, "Nearest Clamp Sampler");
    return sampler;
}

std::unique_ptr<VulkanSampler> createLinearRepeatSampler(
    const VulkanDevice& device, const float anisotropy, const float maxLod)
{
    auto sampler = std::make_unique<VulkanSampler>(
        device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, anisotropy, maxLod);
    device.getDebugMarker().setObjectName(*sampler, "Linear Repeat Sampler");
    return sampler;
}

} // namespace crisp
