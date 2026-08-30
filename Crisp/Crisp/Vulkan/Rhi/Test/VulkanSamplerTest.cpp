#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(VulkanSamplerTest, CreatesLatLongEnvironmentInfo) {
    const auto info = createLatLongEnvironmentSamplerCreateInfo();

    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    EXPECT_EQ(info.minFilter, VK_FILTER_LINEAR);
    EXPECT_EQ(info.magFilter, VK_FILTER_LINEAR);
    EXPECT_EQ(info.mipmapMode, VK_SAMPLER_MIPMAP_MODE_NEAREST);
    EXPECT_EQ(info.addressModeU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    EXPECT_EQ(info.addressModeV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    EXPECT_EQ(info.addressModeW, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    EXPECT_EQ(info.anisotropyEnable, VK_FALSE);
    EXPECT_FLOAT_EQ(info.maxAnisotropy, 1.0f);
    EXPECT_FLOAT_EQ(info.minLod, 0.0f);
    EXPECT_FLOAT_EQ(info.maxLod, 0.0f);
}

TEST(VulkanSamplerTest, ConvenienceInfosMatchLegacyPresets) {
    const auto linearClamp = createLinearClampSamplerCreateInfo();
    EXPECT_EQ(linearClamp.minFilter, VK_FILTER_LINEAR);
    EXPECT_EQ(linearClamp.magFilter, VK_FILTER_LINEAR);
    EXPECT_EQ(linearClamp.mipmapMode, VK_SAMPLER_MIPMAP_MODE_LINEAR);
    EXPECT_EQ(linearClamp.addressModeU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    EXPECT_EQ(linearClamp.addressModeV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    EXPECT_EQ(linearClamp.addressModeW, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    EXPECT_EQ(linearClamp.anisotropyEnable, VK_FALSE);

    const auto nearestClamp = createNearestClampSamplerCreateInfo();
    EXPECT_EQ(nearestClamp.minFilter, VK_FILTER_NEAREST);
    EXPECT_EQ(nearestClamp.magFilter, VK_FILTER_NEAREST);
    EXPECT_EQ(nearestClamp.addressModeU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    const auto linearRepeat = createLinearRepeatSamplerCreateInfo(4.0f, 12.0f);
    EXPECT_EQ(linearRepeat.addressModeU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    EXPECT_EQ(linearRepeat.addressModeV, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    EXPECT_EQ(linearRepeat.addressModeW, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    EXPECT_EQ(linearRepeat.anisotropyEnable, VK_TRUE);
    EXPECT_FLOAT_EQ(linearRepeat.maxAnisotropy, 4.0f);
    EXPECT_FLOAT_EQ(linearRepeat.maxLod, 12.0f);
}

} // namespace
} // namespace crisp
