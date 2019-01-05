#include "ApplicationEnvironment.hpp"

#include "Core/LuaConfig.hpp"

namespace crisp
{
    namespace
    {
        void glfwErrorHandler(int errorCode, const char* message)
        {
            logError("GLFW error code: {}\n{}\n", errorCode, message);
        }
    }

    std::filesystem::path ApplicationEnvironment::ResourcesPath;

    ApplicationEnvironment::ApplicationEnvironment(int, char**)
    {
        ConsoleColorizer::saveDefault();

        glfwSetErrorCallback(glfwErrorHandler);

        if (glfwInit() == GLFW_FALSE)
            logFatal("Could not initialize GLFW library!\n");


        LuaConfig config("../../Resources/Config.lua");
        ResourcesPath = config.get<std::string>("resourcesPath").value_or("../../Resources");
    }

    ApplicationEnvironment::~ApplicationEnvironment()
    {
        glfwTerminate();

        ConsoleColorizer::restoreDefault();
    }

    std::filesystem::path ApplicationEnvironment::getResourcesPath()
    {
        return ResourcesPath;
    }

    std::vector<std::string> ApplicationEnvironment::getVulkanExtensions()
    {
        std::vector<std::string> extensions;
        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (unsigned int i = 0; i < glfwExtensionCount; i++)
            extensions.push_back(glfwExtensions[i]);

        return extensions;
    }
}