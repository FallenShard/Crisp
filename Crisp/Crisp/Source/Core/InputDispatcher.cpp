#include "InputDispatcher.hpp"

namespace crisp
{
    InputDispatcher::InputDispatcher(GLFWwindow* window)
        : m_window(window)
    {
        glfwSetWindowUserPointer(m_window, this);
        glfwSetWindowSizeCallback(m_window, InputDispatcher::resizeCallback);
        glfwSetKeyCallback(m_window, InputDispatcher::keyboardCallback);
        glfwSetCursorPosCallback(m_window, InputDispatcher::mouseMoveCallback);
        glfwSetMouseButtonCallback(m_window, InputDispatcher::mouseButtonCallback);
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
            if (dispatcher) dispatcher->keyDown(key, mode);
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
                dispatcher->mouseButtonDown(button, mods, xPos, yPos);
            }
        }
        else if (action == GLFW_RELEASE)
        {
            auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
            if (dispatcher)
            {
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                dispatcher->mouseButtonUp(button, mods, xPos, yPos);
            }
        }
    }
}