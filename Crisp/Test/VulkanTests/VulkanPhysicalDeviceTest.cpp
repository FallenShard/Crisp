#include <VulkanTests/VulkanTest.hpp>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <CrispCore/Result.hpp>

using namespace crisp;

class VulkanPhysicalDeviceTest : public VulkanTest {};

struct VulkanSetupData
{
    std::unique_ptr<Window> window;
    std::unique_ptr<VulkanContext> context;
};

std::pair<VulkanPhysicalDevice, VulkanContext> createPhysicalDevice()
{
    VulkanContext context(nullptr, {}, false);
    VulkanPhysicalDevice physicalDevice(context.selectPhysicalDevice({}).unwrap());
    return std::make_pair(std::move(physicalDevice), std::move(context));
}

auto createPhysicalDeviceWithSurface()
{
    struct {
        Window window{ 0, 0, 200, 200, "unit_test", true };
        VulkanContext context{ window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanExtensions(), false };
    } vulkanInit;

    VulkanPhysicalDevice physicalDevice(vulkanInit.context.selectPhysicalDevice({}).unwrap());
    return std::make_pair(std::move(physicalDevice), std::move(vulkanInit));
}

TEST_F(VulkanPhysicalDeviceTest, ValidHandle)
{
    const auto& [physicalDevice, context] = createPhysicalDevice();
    ASSERT_NE(physicalDevice.getHandle(), nullptr);
}

TEST_F(VulkanPhysicalDeviceTest, Features)
{
    const auto& [physicalDevice, context] = createPhysicalDevice();
    const auto& features = physicalDevice.getFeatures();
    ASSERT_TRUE(features.tessellationShader);
}

TEST_F(VulkanPhysicalDeviceTest, SurfaceCaps)
{
    const auto& [physicalDevice, deps] = createPhysicalDeviceWithSurface();
    const auto queueFamilies = physicalDevice.queryQueueFamilyIndices(deps.context.getSurface());
    ASSERT_TRUE(queueFamilies.presentFamily.has_value());
    ASSERT_TRUE(queueFamilies.graphicsFamily.has_value());
    ASSERT_TRUE(queueFamilies.computeFamily.has_value());
    ASSERT_TRUE(queueFamilies.transferFamily.has_value());

    const auto surfaceSupport = physicalDevice.querySurfaceSupport(deps.context.getSurface());
    ASSERT_FALSE(surfaceSupport.formats.empty());
    ASSERT_EQ(surfaceSupport.presentModes.size(), 4);
}

TEST_F(VulkanPhysicalDeviceTest, CreateLogicalDevice)
{
    const auto& [physicalDevice, deps] = createPhysicalDeviceWithSurface();
    const auto queueConfig = createDefaultQueueConfiguration(deps.context, physicalDevice);
    EXPECT_EQ(queueConfig.createInfos.size(), 3);

    const VkDevice device = physicalDevice.createLogicalDevice(queueConfig);
    ASSERT_NE(device, VK_NULL_HANDLE);
    vkDestroyDevice(device, nullptr);
}

TEST_F(VulkanPhysicalDeviceTest, FindDepthFormat)
{
    const auto& [physicalDevice, vulkanInit] = createPhysicalDeviceWithSurface();
    ASSERT_EQ(physicalDevice.findSupportedDepthFormat().unwrap(), VK_FORMAT_D32_SFLOAT);
}

TEST_F(VulkanPhysicalDeviceTest, MemoryTypes)
{
    const auto& [physicalDevice, deps] = createPhysicalDeviceWithSurface();
    const auto queueConfig = createDefaultQueueConfiguration(deps.context, physicalDevice);
    const VkDevice device = physicalDevice.createLogicalDevice(queueConfig);

    ASSERT_TRUE(physicalDevice.findStagingBufferMemoryType(device).hasValue());
    ASSERT_TRUE(physicalDevice.findDeviceBufferMemoryType(device).hasValue());
    ASSERT_TRUE(physicalDevice.findDeviceImageMemoryType(device).hasValue());

    const uint32_t stagingHeap = physicalDevice.findStagingBufferMemoryType(device).unwrap();
    ASSERT_EQ(stagingHeap, physicalDevice.findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT).unwrap());

    vkDestroyDevice(device, nullptr);
}
