#pragma once

#include <Crisp/Core/Event.hpp>
#include <Crisp/Core/Keyboard.hpp>
#include <Crisp/Core/Mouse.hpp>
#include <Crisp/Math/Headers.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <string>

#include <GLFW/glfw3.h>

namespace crisp {
enum class CursorState {
    Disabled,
    Normal
};

enum class WindowVisibility {
    Hidden,
    Shown
};

enum class EventType : uint32_t {
    None = 0,
    MouseMoved = 1 << 0,
    MouseButtonPressed = 1 << 1,
    MouseButtonReleased = 1 << 2,
    MouseWheelScrolled = 1 << 3,
    MouseEntered = 1 << 4,
    MouseExited = 1 << 5,

    WindowResized = 1 << 6,
    WindowClosed = 1 << 7,
    WindowFocusGained = 1 << 8,
    WindowFocusLost = 1 << 9,
    WindowMinimized = 1 << 10,
    WindowRestored = 1 << 11,

    KeyPressed = 1 << 12,

    AllMouseEvents =
        MouseMoved | MouseButtonPressed | MouseButtonReleased | MouseWheelScrolled | MouseEntered | MouseExited,

    AllWindowEvents =
        WindowResized | WindowClosed | WindowFocusGained | WindowFocusLost | WindowMinimized | WindowRestored,

    AllKeyEvents = KeyPressed,

    AllEvents = AllMouseEvents | AllWindowEvents | AllKeyEvents
};
DECLARE_BITFLAG(EventType)

class Window {
public:
    Window(
        const glm::ivec2& position,
        const glm::ivec2& size,
        const std::string& title,
        WindowVisibility visibility = WindowVisibility::Shown);
    ~Window();

    Window(const Window& other) = delete;
    Window(Window&& other) noexcept;

    Window& operator=(const Window& other) = delete;
    Window& operator=(Window&& other) noexcept;

    std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)> createSurfaceCallback() const;

    static glm::ivec2 getDesktopResolution();
    static void pollEvents();

    bool shouldClose() const;
    void close();

    void setCursorState(CursorState cursorState);
    void setCursorPosition(const glm::vec2& position);
    void setTitle(const std::string& title);
    void setSize(int32_t width, int32_t height);

    glm::ivec2 getSize() const;
    glm::vec2 getCursorPosition() const;

    inline GLFWwindow* getHandle() const {
        return m_window;
    }

    bool isKeyDown(Key key) const;

    void clearAllEvents();

    void enableEvents(BitFlags<EventType> eventMask);
    void disableEvents(BitFlags<EventType> eventMask);
    bool isEventEnabled(EventType eventType) const;

    Event<int, int> resized;
    Event<Key, int> keyPressed;
    Event<double, double> mouseMoved;
    Event<const MouseEventArgs&> mouseButtonPressed;
    Event<const MouseEventArgs&> mouseButtonReleased;
    Event<double> mouseWheelScrolled;
    Event<double, double> mouseEntered;
    Event<double, double> mouseExited;
    Event<> closed;
    Event<> focusGained;
    Event<> focusLost;
    Event<> minimized;
    Event<> restored;

private:
    static void resizeCallback(GLFWwindow* window, int width, int height);
    static void keyboardCallback(GLFWwindow* window, int key, int scanCode, int action, int mode);
    static void mouseMoveCallback(GLFWwindow* window, double xPos, double yPos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void mouseWheelCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void mouseEnterCallback(GLFWwindow* window, int entered);
    static void closeCallback(GLFWwindow* window);
    static void focusCallback(GLFWwindow* window, int isFocused);
    static void iconifyCallback(GLFWwindow* window, int isIconified);
    GLFWwindow* m_window;

    BitFlags<EventType> m_activeEventMask{EventType::AllEvents};
};

class WindowEventGuard {
public:
    WindowEventGuard(Window& window, BitFlags<EventType> eventMask)
        : window(window) {
        window.disableEvents(eventMask);
    }

    ~WindowEventGuard() {
        window.enableEvents(EventType::AllEvents);
    }

    WindowEventGuard(const WindowEventGuard&) = delete;
    WindowEventGuard& operator=(const WindowEventGuard&) = delete;

    WindowEventGuard(WindowEventGuard&&) = delete;
    WindowEventGuard& operator=(WindowEventGuard&&) = delete;

private:
    Window& window;
};
} // namespace crisp