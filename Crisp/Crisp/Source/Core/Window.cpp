#include "Window.hpp"
#include "Core/EventHub.hpp"

#include <glfw/glfw3.h>

namespace crisp
{
    Window::Window(const glm::ivec2& position, const glm::ivec2& size, std::string title)
        : Window(position.x, position.y, size.x, size.y, title)
    {
    }

    Window::Window(int x, int y, int width, int height, std::string title)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetWindowPos(m_window, x, y);

        m_eventHub = std::make_unique<EventHub>(m_window);

        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_window);
    }

    glm::ivec2 Window::getDesktopResolution()
    {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        return { mode->width, mode->height };
    }

    void Window::pollEvents()
    {
        glfwPollEvents();
    }

    bool Window::shouldClose() const
    {
        return glfwWindowShouldClose(m_window);
    }

    void Window::close()
    {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    void Window::setCursorState(CursorState cursorState)
    {
        int value = cursorState == CursorState::Normal ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
        glfwSetInputMode(m_window, GLFW_CURSOR, value);
    }

    void Window::setCursorPosition(const glm::vec2& position)
    {
        glfwSetCursorPos(m_window, position.x, position.y);
    }

    void Window::setTitle(const std::string& title)
    {
        glfwSetWindowTitle(m_window, title.c_str());
    }

    glm::ivec2 Window::getSize() const
    {
        glm::ivec2 size;
        glfwGetWindowSize(m_window, &size.x, &size.y);
        return size;
    }

    glm::vec2 Window::getCursorPosition() const
    {
        double x, y;
        glfwGetCursorPos(m_window, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y) };
    }

    EventHub& Window::getEventHub()
    {
        return *m_eventHub;
    }

    VkResult Window::createRenderingSurface(VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) const
    {
        return glfwCreateWindowSurface(instance, m_window, allocCallbacks, surface);
    }
}