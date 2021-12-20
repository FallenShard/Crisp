#pragma once

#include <Crisp/Core/Keyboard.hpp>
#include <Crisp/Core/Mouse.hpp>

#include <CrispCore/Event.hpp>
#include <CrispCore/Math/Headers.hpp>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace crisp
{
enum class CursorState
{
    Disabled,
    Normal
};

class EventHub;

class Window
{
public:
    Window(const glm::ivec2& position, const glm::ivec2& size, std::string title, bool hidden = false);
    Window(int x, int y, int width, int height, std::string title, bool hidden = false);
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

    glm::ivec2 getSize() const;
    glm::vec2 getCursorPosition() const;
    inline GLFWwindow* getHandle() const
    {
        return m_window;
    }

    bool isKeyDown(Key key) const;

    void clearAllEvents();

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

private:
    static void resizeCallback(GLFWwindow* window, int width, int height);
    static void keyboardCallback(GLFWwindow* window, int key, int scanCode, int action, int mode);
    static void mouseMoveCallback(GLFWwindow* window, double xPos, double yPos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void mouseWheelCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void mouseEnterCallback(GLFWwindow* window, int entered);
    static void closeCallback(GLFWwindow* window);
    static void focusCallback(GLFWwindow* window, int isFocused);
    GLFWwindow* m_window;
};
} // namespace crisp