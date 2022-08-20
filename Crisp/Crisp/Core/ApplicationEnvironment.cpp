#include <Crisp/Core/ApplicationEnvironment.hpp>

#include <Crisp/ChromeProfiler.hpp>
#include <Crisp/Common/Logger.hpp>
#include <Crisp/LuaConfig.hpp>

#include <GLFW/glfw3.h>
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

void setSpdlogLevel(const std::string_view level)
{
    if (level == "critical")
        spdlog::set_level(spdlog::level::critical);
    else if (level == "error")
        spdlog::set_level(spdlog::level::err);
    else if (level == "warning")
        spdlog::set_level(spdlog::level::warn);
    else if (level == "info")
        spdlog::set_level(spdlog::level::info);
    else if (level == "debug")
        spdlog::set_level(spdlog::level::debug);
    else if (level == "trace")
        spdlog::set_level(spdlog::level::trace);
    else
        spdlog::set_level(spdlog::level::info);
}

} // namespace

std::filesystem::path ApplicationEnvironment::ResourcesPath;
std::filesystem::path ApplicationEnvironment::ShaderSourcesPath;
ApplicationEnvironment::Parameters ApplicationEnvironment::Arguments{};

ApplicationEnvironment::ApplicationEnvironment(Parameters&& parameters)
{
    Arguments = std::move(parameters);
    ChromeProfiler::setThreadName("Main Thread");

    spdlog::set_pattern("%^[%T.%e][%n][%l][Tid: %t]:%$ %v");
    setSpdlogLevel(Arguments.logLevel);
    spdlog::info("Current path: {}", std::filesystem::current_path().string());

    glfwSetErrorCallback(glfwErrorHandler);
    if (glfwInit() == GLFW_FALSE)
    {
        logger->critical("Could not initialize GLFW library!\n");
        std::exit(-1);
    }

    LuaConfig lua;
    lua.openFile(Arguments.configPath);
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

std::filesystem::path ApplicationEnvironment::getShaderSourceDirectory()
{
    return ShaderSourcesPath;
}

std::vector<std::string> ApplicationEnvironment::getRequiredVulkanInstanceExtensions()
{
    std::vector<std::string> extensions;
    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (unsigned int i = 0; i < glfwExtensionCount; i++)
        extensions.push_back(glfwExtensions[i]);

    return extensions;
}

const ApplicationEnvironment::Parameters& ApplicationEnvironment::getParameters()
{
    return Arguments;
}

Result<ApplicationEnvironment::Parameters> parse(int argc, char** argv)
{
    ApplicationEnvironment::Parameters args{};
    CommandLineParser parser{};
    parser.addOption("config", args.configPath, true);
    parser.addOption("scene", args.defaultSceneIndex);
    parser.addOption("enable_ray_tracing", args.enableRayTracingExtension);
    parser.addOption("log_level", args.logLevel);
    if (!parser.parse(argc, argv).isValid())
        return resultError("Failed to parse input arguments!");

    return args;
}

} // namespace crisp
