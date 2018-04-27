#include "Window.hpp"

#include "InputDispatcher.hpp"

namespace crisp
{
    Window::Window(const glm::ivec2& position, const glm::ivec2 & size, std::string title)
        : Window(position.x, position.y, size.x, size.y, title)
    {
    }

    Window::Window(int x, int y, int width, int height, std::string title)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetWindowPos(m_window, x, y);

        m_inputDispatcher = std::make_unique<InputDispatcher>(m_window);
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

    void Window::setInputMode(int mode, int value)
    {
        glfwSetInputMode(m_window, mode, value);
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

    InputDispatcher* Window::getInputDispatcher()
    {
        return m_inputDispatcher.get();
    }

    std::vector<std::string> Window::getVulkanExtensions()
    {
        std::vector<std::string> extensions;
        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (unsigned int i = 0; i < glfwExtensionCount; i++)
            extensions.push_back(glfwExtensions[i]);

        return extensions;
    }

    VkResult Window::createRenderingSurface(VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) const
    {
        return glfwCreateWindowSurface(instance, m_window, allocCallbacks, surface);
    }
}