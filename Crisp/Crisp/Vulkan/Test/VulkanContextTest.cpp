#include <Crisp/Vulkan/Test/VulkanTest.hpp>

namespace crisp {
namespace {

using ::testing::IsNull;
using ::testing::Not;

TEST(VulkanContextTest, WithoutSurface) {
    const VulkanContext context(nullptr, {}, false);
    EXPECT_THAT(context.getSurface(), IsNull());
    EXPECT_THAT(context.getInstance(), Not(IsNull()));
}

TEST(VulkanContextTest, WithoutSurfaceWithValidation) {
    const VulkanContext context(nullptr, {}, true);
    EXPECT_THAT(context.getSurface(), IsNull());
    EXPECT_THAT(context.getInstance(), Not(IsNull()));
}

TEST(VulkanContextTest, WithSurface) {
    glfwInit();
    {
        const Window window(glm::ivec2{0, 0}, glm::ivec2{200, 200}, "unit_test", WindowVisibility::Hidden);
        const VulkanContext context(
            window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), false);
        EXPECT_THAT(context.getSurface(), Not(IsNull()));
        EXPECT_THAT(context.getInstance(), Not(IsNull()));
    }
    glfwTerminate();
}

TEST(VulkanContextTest, DeviceSelection) {
    const VulkanContext context(nullptr, {}, false);
    EXPECT_THAT(context.getSurface(), IsNull());
    EXPECT_THAT(context.getInstance(), Not(IsNull()));
    EXPECT_THAT(context.selectPhysicalDevice({}), Not(HasError()));
}
} // namespace
} // namespace crisp