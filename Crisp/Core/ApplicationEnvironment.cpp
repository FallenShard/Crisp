#include "ApplicationEnvironment.hpp"

#include <CrispCore/LuaConfig.hpp>
#include <CrispCore/ChromeProfiler.hpp>
#include <CrispCore/CommandLineParser.hpp>

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
    CommandLineParser ApplicationEnvironment::CommandLineParser;

    ApplicationEnvironment::ApplicationEnvironment(int argc, char** argv)
    {
        std::vector<std::string_view> commandLineArgs(argc);
        for (uint32_t i = 0; i < argc; ++i)
            commandLineArgs[i] = std::string_view(argv[i]);

        spdlog::set_pattern("[%T.%e][%n][%^%l%$][Tid: %t]: %v");

        CommandLineParser.addOption<std::string>("config", ".");
        CommandLineParser.addOption<uint32_t>("scene", 5);
        CommandLineParser.parse(commandLineArgs);


        spdlog::set_level(spdlog::level::debug);

        ChromeProfiler::setThreadName("Main Thread");

        glfwSetErrorCallback(glfwErrorHandler);

        if (glfwInit() == GLFW_FALSE)
        {
            logger->critical("Could not initialize GLFW library!\n");
            std::exit(-1);
        }

        LuaConfig lua;
        lua.openFile(CommandLineParser.get<std::string>("config"));

        ResourcesPath = lua.get<std::string>("resourcesPath").value();
        ShaderSourcesPath = lua.get<std::string>("shaderSourcesPath").value();
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
