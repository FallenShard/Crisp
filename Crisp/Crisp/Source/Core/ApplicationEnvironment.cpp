#include "ApplicationEnvironment.hpp"

#include <iostream>

namespace crisp
{
    ApplicationEnvironment::ApplicationEnvironment(int argc, char** argv)
    {
        ConsoleColorizer::saveDefault();
        
        if (glfwInit() == GLFW_FALSE)
        {
            std::cerr << "Could not initialize GLFW library!\n";
        }
    }

    ApplicationEnvironment::~ApplicationEnvironment()
    {
        glfwTerminate();

        ConsoleColorizer::restoreDefault();
    }
}