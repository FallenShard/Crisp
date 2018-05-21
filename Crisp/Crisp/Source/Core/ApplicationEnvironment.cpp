#include "ApplicationEnvironment.hpp"

#include <iostream>

namespace crisp
{
    namespace
    {
        void glfwErrorHandler(int errorCode, const char* message)
        {
            ConsoleColorizer colorizer(ConsoleColor::LightRed);
            std::cout << "GLFW error code: " << errorCode << std::endl;
            std::cout << message << std::endl;
        }
    }

    ApplicationEnvironment::ApplicationEnvironment(int argc, char** argv)
    {
        ConsoleColorizer::saveDefault();

        if (glfwInit() == GLFW_FALSE)
        {
            std::cerr << "Could not initialize GLFW library!\n";
        }
        glfwSetErrorCallback(glfwErrorHandler);
    }

    ApplicationEnvironment::~ApplicationEnvironment()
    {
        glfwTerminate();

        ConsoleColorizer::restoreDefault();
    }
}