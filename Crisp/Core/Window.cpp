#include "Window.hpp"

#include "InputTranslator.hpp"

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

        glfwSetWindowUserPointer(m_window, this);
        glfwSetWindowSizeCallback(m_window, resizeCallback);
        glfwSetWindowCloseCallback(m_window, closeCallback);
        glfwSetKeyCallback(m_window, keyboardCallback);
        glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
        glfwSetScrollCallback(m_window, mouseWheelCallback);
        glfwSetCursorPosCallback(m_window, mouseMoveCallback);
        glfwSetCursorEnterCallback(m_window, mouseEnterCallback);

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

    VkResult Window::createRenderingSurface(VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) const
    {
        return glfwCreateWindowSurface(instance, m_window, allocCallbacks, surface);
    }

    bool Window::isKeyDown(Key key) const
    {
        return glfwGetKey(m_window, translateKeyToGlfw(key)) == GLFW_PRESS;
    }

    void Window::clearAllEvents()
    {
        resized.clear();
        keyPressed.clear();
        mouseMoved.clear();
        mouseButtonPressed.clear();
        mouseButtonReleased.clear();
        mouseWheelScrolled.clear();
        mouseEntered.clear();
        mouseExited.clear();
        closed.clear();
        focusGained.clear();
        focusLost.clear();
    }

    void Window::resizeCallback(GLFWwindow* window, int width, int height)
    {
        auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->resized(width, height);
    }

    void Window::keyboardCallback(GLFWwindow* window, int key, int /*scanCode*/, int action, int mode)
    {
        if (action == GLFW_PRESS)
        {
            auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            if (dispatcher) dispatcher->keyPressed(translateGlfwToKey(key), mode);
        }
    }

    void Window::mouseMoveCallback(GLFWwindow* window, double xPos, double yPos)
    {
        auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->mouseMoved(xPos, yPos);
    }

    void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        if (action == GLFW_PRESS)
        {
            auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            if (dispatcher)
            {
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                dispatcher->mouseButtonPressed({ translateGlfwToMouseButton(button), ModifierFlags(mods), xPos, yPos });
            }
        }
        else if (action == GLFW_RELEASE)
        {
            auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            if (dispatcher)
            {
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                dispatcher->mouseButtonReleased({ translateGlfwToMouseButton(button), ModifierFlags(mods), xPos, yPos });
            }
        }
    }

    void Window::mouseWheelCallback(GLFWwindow* window, double /*xOffset*/, double yOffset)
    {
        auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->mouseWheelScrolled(yOffset);
    }

    void Window::mouseEnterCallback(GLFWwindow* window, int entered)
    {
        auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (dispatcher)
        {
            double xPos, yPos;
            glfwGetCursorPos(window, &xPos, &yPos);
            if (entered)
                dispatcher->mouseEntered(xPos, yPos);
            else
                dispatcher->mouseExited(xPos, yPos);
        }
    }

    void Window::closeCallback(GLFWwindow* window)
    {
        auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->closed();
    }

    void Window::focusCallback(GLFWwindow* window, int isFocused)
    {
        auto dispatcher = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        if (dispatcher)
        {
            if (isFocused)
                dispatcher->focusGained();
            else
                dispatcher->focusLost();
        }
    }
}