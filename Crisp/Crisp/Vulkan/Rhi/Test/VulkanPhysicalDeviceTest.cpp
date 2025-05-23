#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {
using VulkanPhysicalDeviceWithSurfaceTest = VulkanTestWithSurface;
using VulkanPhysicalDeviceTest = VulkanTest;

using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::SizeIs;

TEST(VulkanPhysicalDeviceSelectionTest, DeviceSelection) {
    const VulkanInstance instance(nullptr, {}, false);
    EXPECT_THAT(instance.getSurface(), IsNull());
    EXPECT_THAT(instance.getHandle(), Not(IsNull()));
    EXPECT_THAT(selectPhysicalDevice(instance, {}), Not(HasError()));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, SurfaceCapabilities) {
    const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}).unwrap());
    const auto queueFamilies = physicalDevice.queryQueueFamilyIndices(instance_->getSurface());
    EXPECT_TRUE(queueFamilies.presentFamily.has_value());
    EXPECT_TRUE(queueFamilies.graphicsFamily.has_value());
    EXPECT_TRUE(queueFamilies.computeFamily.has_value());
    EXPECT_TRUE(queueFamilies.transferFamily.has_value());

    const auto surfaceSupport = physicalDevice.querySurfaceSupport(instance_->getSurface());
    EXPECT_THAT(surfaceSupport.formats, Not(IsEmpty()));
    EXPECT_THAT(surfaceSupport.presentModes, Not(IsEmpty()));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateDefaultQueueConfiguration) {
    const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}).unwrap());
    const auto queueConfig = createDefaultQueueConfiguration(*instance_, physicalDevice);
    EXPECT_THAT(queueConfig.createInfos, SizeIs(3));
    EXPECT_THAT(queueConfig.priorities, SizeIs(Ge(3)));
    EXPECT_THAT(queueConfig.identifiers, SizeIs(3));
    EXPECT_THAT(queueConfig.types, SizeIs(3));
}

TEST_F(VulkanPhysicalDeviceWithSurfaceTest, CreateLogicalDevice) {
    const VulkanPhysicalDevice physicalDevice(selectPhysicalDevice(*instance_, {}).unwrap());

    const VkDevice device =
        createLogicalDeviceHandle(physicalDevice, createDefaultQueueConfiguration(*instance_, physicalDevice));
    EXPECT_THAT(device, Not(IsNull()));
    vkDestroyDevice(device, nullptr);
}

TEST_F(VulkanPhysicalDeviceTest, Features) {
    EXPECT_TRUE(physicalDevice_->getFeatures().tessellationShader);
}

TEST_F(VulkanPhysicalDeviceTest, FindDepthFormat) {
    ASSERT_EQ(physicalDevice_->findSupportedDepthFormat().unwrap(), VK_FORMAT_D32_SFLOAT);
}

TEST_F(VulkanPhysicalDeviceTest, MemoryTypes) {
    const auto device = device_->getHandle();
    EXPECT_TRUE(physicalDevice_->findStagingBufferMemoryType(device).hasValue());
    EXPECT_TRUE(physicalDevice_->findDeviceBufferMemoryType(device).hasValue());
    EXPECT_TRUE(physicalDevice_->findDeviceImageMemoryType(device).hasValue());

    EXPECT_EQ(
        physicalDevice_->findStagingBufferMemoryType(device).unwrap(),
        physicalDevice_->findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT).unwrap());
}

TEST_F(VulkanPhysicalDeviceTest, FormatProperties) {
    const auto props = physicalDevice_->getFormatProperties(VK_FORMAT_D32_SFLOAT);
    EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
} // namespace
} // namespace crisp