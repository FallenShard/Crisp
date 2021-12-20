#include <Crisp/Core/ApplicationEnvironment.hpp>

#include <CrispCore/ChromeProfiler.hpp>
#include <CrispCore/CommandLineParser.hpp>
#include <CrispCore/LuaConfig.hpp>

#include <GLFW/glfw3.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

namespace crisp
{
namespace
{
auto logger = spdlog::stdout_color_mt("ApplicationEnvironment");

void glfwErrorHandler(int errorCode, const char* message)
{
    logger->error("GLFW error code: {}. Message: {}", errorCode, message);
}
} // namespace

std::filesystem::path ApplicationEnvironment::ResourcesPath;
std::filesystem::path ApplicationEnvironment::ShaderSourcesPath;
uint32_t ApplicationEnvironment::DefaultSceneIdx{ 4 };

ApplicationEnvironment::ApplicationEnvironment(int argc, char** argv)
{
    spdlog::set_pattern("%^[%T.%e][%n][%l][Tid: %t]:%$ %v");
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Current path: {}", std::filesystem::current_path().string());

    std::filesystem::path configPath{};
    CommandLineParser cmdLineParser{};
    cmdLineParser.addOption("config", configPath, true);
    cmdLineParser.addOption("scene", DefaultSceneIdx);
    cmdLineParser.parse(argc, argv).unwrap();

    ChromeProfiler::setThreadName("Main Thread");

    glfwSetErrorCallback(glfwErrorHandler);

    if (glfwInit() == GLFW_FALSE)
    {
        logger->critical("Could not initialize GLFW library!\n");
        std::exit(-1);
    }

    LuaConfig lua;
    lua.openFile(configPath);
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

uint32_t ApplicationEnvironment::getDefaultSceneIdx()
{
    return DefaultSceneIdx;
}
} // namespace crisp
