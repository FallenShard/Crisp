#pragma once

#include <GLFW/glfw3.h>

#include <Event.hpp>

namespace crisp
{
    class InputDispatcher
    {
    public:
        InputDispatcher(GLFWwindow* window);
        ~InputDispatcher() = default;

        InputDispatcher(const InputDispatcher& other) = delete;
        InputDispatcher& operator=(const InputDispatcher& other) = delete;
        InputDispatcher& operator=(InputDispatcher&& other) = delete;

        static void resizeCallback(GLFWwindow* window, int width, int height);
        Event<void, int, int> windowResized;

        static void keyboardCallback(GLFWwindow* window, int key, int scanCode, int action, int mode);
        Event<void, int, int> keyDown;

        static void mouseMoveCallback(GLFWwindow* window, double xPos, double yPos);
        Event<void, double, double> mouseMoved;
        
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        Event<void, int, int, double, double> mouseButtonDown;

    private:
        GLFWwindow* m_window;
    };
}