#include "ApplicationEnvironment.hpp"

#include "Core/LuaConfig.hpp"

#include <CrispCore/ChromeProfiler.hpp>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace crisp
{
    namespace
    {
        auto logger = spdlog::stdout_color_mt("ApplicationEnvironment");

        void glfwErrorHandler(int errorCode, const char* message)
        {
            logger->error("GLFW error code: {}. Message: {}", errorCode, message);
        }
    }

    std::filesystem::path ApplicationEnvironment::ResourcesPath;
    std::filesystem::path ApplicationEnvironment::ShaderSourcesPath;

    ApplicationEnvironment::ApplicationEnvironment(int /*argc*/, char** /*argv*/)
    {
        spdlog::set_pattern("[%T.%e][%n][%^%l%$][Tid: %t]: %v");
        spdlog::set_level(spdlog::level::debug);

        ChromeProfiler::setThreadName("Main Thread");

        glfwSetErrorCallback(glfwErrorHandler);

        if (glfwInit() == GLFW_FALSE)
        {
            logger->critical("Could not initialize GLFW library!\n");
            std::exit(-1);
        }

        const LuaConfig config("../../Config.lua");
        ResourcesPath = config.get<std::string>("resourcesPath").value();
        ShaderSourcesPath = config.get<std::string>("shaderSourcesPath").value();
    }

    ApplicationEnvironment::~ApplicationEnvironment()
    {
        glfwTerminate();

        ChromeProfiler::flushThreadBuffer();
        ChromeProfiler::finalize();
    }

    std::filesystem::path ApplicationEnvironment::getResourcesPath()
    {
        return ResourcesPath;
    }

    std::filesystem::path ApplicationEnvironment::getShaderSourcesPath()
    {
        return ShaderSourcesPath;
    }

    std::vector<std::string> ApplicationEnvironment::getRequiredVulkanExtensions()
    {
        std::vector<std::string> extensions;
        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (unsigned int i = 0; i < glfwExtensionCount; i++)
            extensions.push_back(glfwExtensions[i]);

        return extensions;
    }
}
