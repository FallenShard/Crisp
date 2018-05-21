#pragma once

#include <CrispCore/Event.hpp>

#include "Keyboard.hpp"
#include "Mouse.hpp"

struct GLFWwindow;

namespace crisp
{
    class InputDispatcher
    {
    public:
        InputDispatcher(GLFWwindow* window);
        ~InputDispatcher() = default;

        bool isKeyDown(Key key) const;

        InputDispatcher(const InputDispatcher& other)            = delete;
        InputDispatcher& operator=(const InputDispatcher& other) = delete;
        InputDispatcher& operator=(InputDispatcher&& other)      = delete;

        Event<int, int> windowResized;
        Event<Key, int> keyPressed;
        Event<double, double> mouseMoved;
        Event<const MouseEventArgs&> mouseButtonPressed;
        Event<const MouseEventArgs&> mouseButtonReleased;
        Event<double> mouseWheelScrolled;
        Event<double, double> mouseEntered;
        Event<double, double> mouseExited;
        Event<> windowClosed;

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
}