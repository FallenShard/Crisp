#include "Window.hpp"

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

    GLFWwindow* Window::getHandle() const
    {
        return m_window;
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