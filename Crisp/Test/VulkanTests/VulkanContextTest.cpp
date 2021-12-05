#include <VulkanTests/VulkanTest.hpp>

#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>

using namespace crisp;

class VulkanContextTest : public VulkanTest {};

std::pair<VulkanContext, Window> createContextWithSurface()
{
    Window window = Window({ 0, 0 }, { 200, 200 }, "unit_test", true);
    return { VulkanContext(window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanExtensions(), false), std::move(window) };
}

TEST_F(VulkanContextTest, WithoutSurface)
{
    VulkanContext context(nullptr, {}, false);
    ASSERT_EQ(context.getSurface(), VK_NULL_HANDLE);
}

TEST_F(VulkanContextTest, WithSurface)
{
    auto [context, window] = createContextWithSurface();
    ASSERT_NE(context.getSurface(), VK_NULL_HANDLE);
}

TEST_F(VulkanContextTest, DeviceSelection)
{
    VulkanContext context(nullptr, {}, false);
    ASSERT_EQ(context.getSurface(), VK_NULL_HANDLE);

    const auto device = context.selectPhysicalDevice({});
    EXPECT_TRUE(device.hasValue());
}
