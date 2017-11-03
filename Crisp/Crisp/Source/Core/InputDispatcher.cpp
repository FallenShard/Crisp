#include "InputDispatcher.hpp"

namespace crisp
{
    InputDispatcher::InputDispatcher(GLFWwindow* window)
        : m_window(window)
    {
        glfwSetWindowUserPointer(m_window, this);
        glfwSetWindowSizeCallback(m_window,  InputDispatcher::resizeCallback);
        glfwSetKeyCallback(m_window,         InputDispatcher::keyboardCallback);
        glfwSetCursorPosCallback(m_window,   InputDispatcher::mouseMoveCallback);
        glfwSetMouseButtonCallback(m_window, InputDispatcher::mouseButtonCallback);
        glfwSetWindowCloseCallback(m_window, InputDispatcher::closeCallback);
        glfwSetScrollCallback(m_window,      InputDispatcher::mouseWheelCallback);
        glfwSetCursorEnterCallback(m_window, InputDispatcher::mouseEnterCallback);
    }

    GLFWwindow* InputDispatcher::getWindow() const
    {
        return m_window;
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
            if (dispatcher) dispatcher->keyPressed(key, mode);
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
                dispatcher->mouseButtonPressed(button, mods, xPos, yPos);
            }
        }
        else if (action == GLFW_RELEASE)
        {
            auto dispatcher = reinterpret_cast<InputDispatcher*>(glfwGetWindowUserPointer(window));
            if (dispatcher)
            {
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                dispatcher->mouseButtonReleased(button, mods, xPos, yPos);
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
}