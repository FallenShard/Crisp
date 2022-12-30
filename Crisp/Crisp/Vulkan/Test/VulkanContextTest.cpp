#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>

using namespace crisp;

class VulkanContextTest : public VulkanTest
{
};

std::pair<std::unique_ptr<VulkanContext>, Window> createContextWithSurface()
{
    Window window(glm::ivec2{0, 0}, glm::ivec2{200, 200}, "unit_test", WindowVisibility::Hidden);
    return {
        std::make_unique<VulkanContext>(
            window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), false),
        std::move(window)};
}

TEST_F(VulkanContextTest, WithoutSurface)
{
    VulkanContext context(nullptr, {}, false);
    ASSERT_EQ(context.getSurface(), VK_NULL_HANDLE);
}

TEST_F(VulkanContextTest, WithSurface)
{
    auto [context, window] = createContextWithSurface();
    ASSERT_NE(context->getSurface(), VK_NULL_HANDLE);
}

TEST_F(VulkanContextTest, DeviceSelection)
{
    VulkanContext context(nullptr, {}, false);
    ASSERT_EQ(context.getSurface(), VK_NULL_HANDLE);
    EXPECT_TRUE(context.selectPhysicalDevice({}).hasValue());
}
