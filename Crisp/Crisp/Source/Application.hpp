#pragma once

#include <memory>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace crisp
{
    class InputDispatcher;

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

        void onResize(int width, int height);

    private:
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_window;

        std::unique_ptr<InputDispatcher> m_inputDispatcher;

    };
}