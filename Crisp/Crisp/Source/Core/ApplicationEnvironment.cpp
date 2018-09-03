#include "ApplicationEnvironment.hpp"

#include "Core/LuaConfig.hpp"

namespace crisp
{
    namespace
    {
        void glfwErrorHandler(int errorCode, const char* message)
        {
            logError("GLFW error code: ", errorCode, '\n', message);
        }
    }

    ApplicationEnvironment::ApplicationEnvironment(int argc, char** argv)
    {
        ConsoleColorizer::saveDefault();

        glfwSetErrorCallback(glfwErrorHandler);

        if (glfwInit() == GLFW_FALSE)
            logError("Could not initialize GLFW library!");


    }

    ApplicationEnvironment::~ApplicationEnvironment()
    {
        glfwTerminate();

        ConsoleColorizer::restoreDefault();
    }
}