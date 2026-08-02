#include <Crisp/Core/Window.hpp>

#include <type_traits>

#include <gmock/gmock.h>

namespace crisp {
namespace {

constexpr int32_t kDefaultWidth = 200;
constexpr int32_t kDefaultHeight = 200;

TEST(WindowTest, Basic) {
    ASSERT_EQ(glfwInit(), GLFW_TRUE);

    constexpr glm::ivec2 kDefaultSize{kDefaultWidth, kDefaultHeight};

    {
        Window window(glm::ivec2{0, 0}, kDefaultSize, "unit_test", WindowVisibility::Hidden);
        EXPECT_EQ(window.getSize(), kDefaultSize);

        const auto focusCallback = glfwSetWindowFocusCallback(window.getHandle(), nullptr);
        EXPECT_NE(focusCallback, nullptr);
        glfwSetWindowFocusCallback(window.getHandle(), focusCallback);

        window.disableEvents(EventType::KeyPressed);
        {
            const WindowEventGuard guard(window, EventType::AllMouseEvents);
            EXPECT_FALSE(window.isEventEnabled(EventType::MouseMoved));
            EXPECT_FALSE(window.isEventEnabled(EventType::KeyPressed));
        }
        EXPECT_TRUE(window.isEventEnabled(EventType::MouseMoved));
        EXPECT_FALSE(window.isEventEnabled(EventType::KeyPressed));

        window.minimized += [] {};
        window.restored += [] {};
        window.clearAllEvents();
        EXPECT_EQ(window.minimized.getSubscriberCount(), 0);
        EXPECT_EQ(window.restored.getSubscriberCount(), 0);
    }

    glfwTerminate();
}

static_assert(!std::is_move_constructible_v<Window>);
static_assert(!std::is_move_assignable_v<Window>);
} // namespace
} // namespace crisp
