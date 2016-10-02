#pragma once

#include <memory>

#include <glad/glad.h>
#include <GLFW\glfw3.h>

namespace crisp
{
    class Application
    {
    public:
        Application();
        ~Application();

        Application(const Application& other) = delete;
        Application& operator=(const Application& other) = delete;
        Application& operator=(Application&& other) = delete;

        static void initDependencies();

        void run();

    private:
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_window;

    };
}