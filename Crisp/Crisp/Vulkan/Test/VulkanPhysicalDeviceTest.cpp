#include <Crisp/Vulkan/Test/VulkanTest.hpp>

namespace crisp::test
{
namespace
{
using VulkanPhysicalDeviceTest = VulkanTest;

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::SizeIs;

TEST(VulkanPhysicalDeviceWithSurfaceTest, SurfaceCaps)
{
    glfwInit();
    const Window window(glm::ivec2{0, 0}, glm::ivec2{200, 200}, "unit_test", WindowVisibility::Hidden);
    const VulkanContext context(
        window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), false);
    const VulkanPhysicalDevice physicalDevice(context.selectPhysicalDevice({}).unwrap());
    const auto queueFamilies = physicalDevice.queryQueueFamilyIndices(context.getSurface());
    EXPECT_TRUE(queueFamilies.presentFamily.has_value());
    EXPECT_TRUE(queueFamilies.graphicsFamily.has_value());
    EXPECT_TRUE(queueFamilies.computeFamily.has_value());
    EXPECT_TRUE(queueFamilies.transferFamily.has_value());

    const auto surfaceSupport = physicalDevice.querySurfaceSupport(context.getSurface());
    EXPECT_THAT(surfaceSupport.formats, Not(IsEmpty()));
    EXPECT_THAT(surfaceSupport.presentModes, SizeIs(4));
}

TEST(VulkanPhysicalDeviceWithSurfaceTest, CreateDevice)
{
    glfwInit();
    const Window window(glm::ivec2{0, 0}, glm::ivec2{200, 200}, "unit_test", WindowVisibility::Hidden);
    const VulkanContext context(
        window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), false);
    const VulkanPhysicalDevice physicalDevice(context.selectPhysicalDevice({}).unwrap());
    const auto queueConfig = createDefaultQueueConfiguration(context, physicalDevice);
    EXPECT_THAT(createDefaultQueueConfiguration(context, physicalDevice).createInfos, SizeIs(3));

    const VkDevice device = createLogicalDeviceHandle(physicalDevice, queueConfig);
    EXPECT_THAT(device, Not(IsNull()));
    vkDestroyDevice(device, nullptr);
}

TEST_F(VulkanPhysicalDeviceTest, Features)
{
    const auto& features = physicalDevice_->getFeatures();
    EXPECT_TRUE(features.tessellationShader);
}

TEST_F(VulkanPhysicalDeviceTest, FindDepthFormat)
{
    ASSERT_EQ(physicalDevice_->findSupportedDepthFormat().unwrap(), VK_FORMAT_D32_SFLOAT);
}

TEST_F(VulkanPhysicalDeviceTest, MemoryTypes)
{
    const auto device = device_->getHandle();
    EXPECT_TRUE(physicalDevice_->findStagingBufferMemoryType(device).hasValue());
    EXPECT_TRUE(physicalDevice_->findDeviceBufferMemoryType(device).hasValue());
    EXPECT_TRUE(physicalDevice_->findDeviceImageMemoryType(device).hasValue());

    EXPECT_EQ(
        physicalDevice_->findStagingBufferMemoryType(device).unwrap(),
        physicalDevice_->findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT).unwrap());
}

TEST_F(VulkanPhysicalDeviceTest, FormatProperties)
{
    const auto props = physicalDevice_->getFormatProperties(VK_FORMAT_D32_SFLOAT);
    EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    EXPECT_TRUE(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
} // namespace
} // namespace crisp::test