#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>

#include <gmock/gmock.h>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>

namespace crisp {
namespace {

using ::testing::IsNull;
using ::testing::Not;

TEST(VulkanInstanceTest, WithoutSurface) {
    setDefaultSpdlogPattern();
    const VulkanInstance instance(nullptr, {}, false);
    EXPECT_THAT(instance.getSurface(), IsNull());
    EXPECT_THAT(instance.getHandle(), Not(IsNull()));
}

TEST(VulkanInstanceTest, WithoutSurfaceWithValidation) {
    const VulkanInstance instance(nullptr, {}, true);
    EXPECT_THAT(instance.getSurface(), IsNull());
    EXPECT_THAT(instance.getHandle(), Not(IsNull()));
}

TEST(VulkanInstanceTest, WithSurface) {
    glfwInit();
    {
        const Window window(glm::ivec2{0, 0}, glm::ivec2{200, 200}, "unit_test", WindowVisibility::Hidden);
        const VulkanInstance instance(
            window.createSurfaceCallback(), ApplicationEnvironment::getRequiredVulkanInstanceExtensions(), false);
        EXPECT_THAT(instance.getSurface(), Not(IsNull()));
        EXPECT_THAT(instance.getHandle(), Not(IsNull()));
    }
    glfwTerminate();
}

} // namespace
} // namespace crisp