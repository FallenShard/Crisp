#include <Crisp/Core/Window.hpp>

#include <gmock/gmock.h>

namespace crisp {
namespace {

constexpr int32_t kDefaultWidth = 200;
constexpr int32_t kDefaultHeight = 200;

TEST(WindowTest, Basic) {
    glfwInit();

    constexpr glm::ivec2 kDefaultSize{kDefaultWidth, kDefaultHeight};

    const Window window(glm::ivec2{0, 0}, kDefaultSize, "unit_test", WindowVisibility::Hidden);
    EXPECT_EQ(window.getSize(), kDefaultSize);
}
} // namespace
} // namespace crisp