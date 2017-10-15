#pragma once

#include <GLFW/glfw3.h>

#include <CrispCore/Event.hpp>

namespace crisp
{
    class InputDispatcher
    {
    public:
        InputDispatcher(GLFWwindow* window);
        ~InputDispatcher() = default;

        GLFWwindow* getWindow() const;

        InputDispatcher(const InputDispatcher& other)            = delete;
        InputDispatcher& operator=(const InputDispatcher& other) = delete;
        InputDispatcher& operator=(InputDispatcher&& other)      = delete;

        static void resizeCallback(GLFWwindow* window, int width, int height);
        Event<int, int> windowResized;

        static void keyboardCallback(GLFWwindow* window, int key, int scanCode, int action, int mode);
        Event<int, int> keyPressed;

        static void mouseMoveCallback(GLFWwindow* window, double xPos, double yPos);
        Event<double, double> mouseMoved;
        
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        Event<int, int, double, double> mouseButtonPressed;
        Event<int, int, double, double> mouseButtonReleased;

        static void mouseWheelCallback(GLFWwindow* window, double xOffset, double yOffset);
        Event<double> mouseWheelScrolled;

        static void closeCallback(GLFWwindow* window);
        Event<> windowClosed;

    private:
        GLFWwindow* m_window;
    };
}