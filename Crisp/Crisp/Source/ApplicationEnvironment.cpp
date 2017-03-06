#include "ApplicationEnvironment.hpp"

#include <iostream>

namespace crisp
{
    ApplicationEnvironment::ApplicationEnvironment()
    {
        if (glfwInit() == GLFW_FALSE)
        {
            std::cerr << "Could not initialize GLFW library!" << std::endl;
        }
    }

    ApplicationEnvironment::~ApplicationEnvironment()
    {
        glfwTerminate();
    }
}