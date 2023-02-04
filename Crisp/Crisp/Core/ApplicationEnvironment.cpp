#include <Crisp/Core/ApplicationEnvironment.hpp>

#include <Crisp/Common/Logger.hpp>
#include <Crisp/IO/JsonUtils.hpp>
#include <Crisp/Utils/ChromeProfiler.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>
#include <GLFW/glfw3.h>

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

ApplicationEnvironment::ApplicationEnvironment(Parameters&& parameters)
{
    m_arguments = std::move(parameters);
    ChromeProfiler::setThreadName("Main Thread");

    spdlog::set_pattern("%^[%T.%e][%n][%l][%t]:%$ %v");
    setSpdlogLevel(m_arguments.logLevel);
    logger->info("Current path: {}", std::filesystem::current_path().string());

    glfwSetErrorCallback(glfwErrorHandler);
    if (glfwInit() == GLFW_FALSE)
    {
        logger->critical("Could not initialize GLFW library!\n");
        std::terminate();
    }

    const auto json{loadJsonFromFile(m_arguments.configPath).unwrap()};
    m_resourcesPath = json["resourcesPath"].get<std::string>();
    m_shaderSourcesPath = json["shaderSourcesPath"].get<std::string>();
    m_arguments.scene = json["scene"].get<std::string>();
}

ApplicationEnvironment::~ApplicationEnvironment()
{
    glfwTerminate();

    ChromeProfiler::flushThreadBuffer();
    ChromeProfiler::finalize();
}

const std::filesystem::path& ApplicationEnvironment::getResourcesPath() const
{
    return m_resourcesPath;
}

const std::filesystem::path& ApplicationEnvironment::getShaderSourceDirectory() const
{
    return m_shaderSourcesPath;
}

std::vector<std::string> ApplicationEnvironment::getRequiredVulkanInstanceExtensions()
{
    std::vector<std::string> extensions;
    uint32_t glfwExtensionCount{0};
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (unsigned int i = 0; i < glfwExtensionCount; i++)
        extensions.push_back(glfwExtensions[i]);

    return extensions;
}

const ApplicationEnvironment::Parameters& ApplicationEnvironment::getParameters() const
{
    return m_arguments;
}

Result<ApplicationEnvironment::Parameters> parse(int argc, char** argv)
{
    ApplicationEnvironment::Parameters args{};
    CommandLineParser parser{};
    parser.addOption("config", args.configPath, true);
    parser.addOption("scene", args.scene);
    parser.addOption("enable_ray_tracing", args.enableRayTracingExtension);
    parser.addOption("log_level", args.logLevel);
    if (!parser.parse(argc, argv).isValid())
        return resultError("Failed to parse input arguments!");

    return args;
}

} // namespace crisp
