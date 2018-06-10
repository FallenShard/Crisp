#include "InputDispatcher.hpp"

#include <GLFW/glfw3.h>

#include "InputTranslator.hpp"

namespace crisp
{
    InputDispatcher::InputDispatcher(GLFWwindow* window)
        : m_window(window)
    {
        glfwSetWindowUserPointer(window, this);
        glfwSetWindowSizeCallback(window,  resizeCallback);
        glfwSetWindowCloseCallback(window, closeCallback);
        glfwSetKeyCallback(window,         keyboardCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetScrollCallback(window,      mouseWheelCallback);
        glfwSetCursorPosCallback(window,   mouseMoveCallback);
        glfwSetCursorEnterCallback(window, mouseEnterCallback);
    }

    bool InputDispatcher::isKeyDown(Key key) const
    {
        return glfwGetKey(m_window, translateKeyToGlfw(key)) == GLFW_PRESS;
    }

    void InputDispatcher::resizeCallback(GLFWwindow* window, int width, int height)
    {
        auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->windowResized(width, height);
    }

    void InputDispatcher::keyboardCallback(GLFWwindow* window, int key, int scanCode, int action, int mode)
    {
        if (action == GLFW_PRESS)
        {
            auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
            if (dispatcher) dispatcher->keyPressed(translateGlfwToKey(key), mode);
        }
    }

    void InputDispatcher::mouseMoveCallback(GLFWwindow* window, double xPos, double yPos)
    {
        auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->mouseMoved(xPos, yPos);
    }

    void InputDispatcher::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        if (action == GLFW_PRESS)
        {
            auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
            if (dispatcher)
            {
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                dispatcher->mouseButtonPressed({ translateGlfwToMouseButton(button), ModifierFlags(mods), xPos, yPos });
            }
        }
        else if (action == GLFW_RELEASE)
        {
            auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
            if (dispatcher)
            {
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                dispatcher->mouseButtonReleased({ translateGlfwToMouseButton(button), ModifierFlags(mods), xPos, yPos });
            }
        }
    }

    void InputDispatcher::mouseWheelCallback(GLFWwindow* window, double xOffset, double yOffset)
    {
        auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->mouseWheelScrolled(yOffset);
    }

    void InputDispatcher::mouseEnterCallback(GLFWwindow* window, int entered)
    {
        auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
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

    void InputDispatcher::closeCallback(GLFWwindow* window)
    {
        auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
        if (dispatcher) dispatcher->windowClosed();
    }

    void InputDispatcher::focusCallback(GLFWwindow* window, int isFocused)
    {
        auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
    }
}