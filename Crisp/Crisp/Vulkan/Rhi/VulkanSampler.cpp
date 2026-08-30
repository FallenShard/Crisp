#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
VkSamplerCreateInfo createLinearClampSamplerCreateInfo(const float anisotropy, const float maxLod) {
    return {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = anisotropy == 1.0f ? VK_FALSE : VK_TRUE,
        .maxAnisotropy = anisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = maxLod,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
}

VkSamplerCreateInfo createNearestClampSamplerCreateInfo() {
    return {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
}

VkSamplerCreateInfo createLinearRepeatSamplerCreateInfo(const float anisotropy, const float maxLod) {
    return {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = anisotropy == 1.0f ? VK_FALSE : VK_TRUE,
        .maxAnisotropy = anisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = maxLod,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
}

VkSamplerCreateInfo createLatLongEnvironmentSamplerCreateInfo() {
    return {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
}

VulkanSampler::VulkanSampler(const VulkanDevice& device, const VkSamplerCreateInfo& createInfo)
    : VulkanResource(device.getResourceDeallocator()) {
    VK_FATAL(vkCreateSampler(device.getHandle(), &createInfo, nullptr, &m_handle));
}

std::unique_ptr<VulkanSampler> createLinearClampSampler(
    const VulkanDevice& device, const float anisotropy, const float maxLod) {
    auto sampler = std::make_unique<VulkanSampler>(device, createLinearClampSamplerCreateInfo(anisotropy, maxLod));
    device.setObjectName(*sampler, "Linear Clamp Sampler");
    return sampler;
}

std::unique_ptr<VulkanSampler> createNearestClampSampler(const VulkanDevice& device) {
    auto sampler = std::make_unique<VulkanSampler>(device, createNearestClampSamplerCreateInfo());
    device.setObjectName(*sampler, "Nearest Clamp Sampler");
    return sampler;
}

std::unique_ptr<VulkanSampler> createLinearRepeatSampler(
    const VulkanDevice& device, const float anisotropy, const float maxLod) {
    auto sampler = std::make_unique<VulkanSampler>(device, createLinearRepeatSamplerCreateInfo(anisotropy, maxLod));
    device.setObjectName(*sampler, "Linear Repeat Sampler");
    return sampler;
}

} // namespace crisp
