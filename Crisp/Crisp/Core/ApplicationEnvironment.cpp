#include <Crisp/Core/ApplicationEnvironment.hpp>

#include <GLFW/glfw3.h>

#include <Crisp/Core/CommandLineParser.hpp>
#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("ApplicationEnvironment");

void glfwErrorHandler(const int32_t errorCode, const char* message) {
    CRISP_LOGE("GLFW error code: {}. Message: {}", errorCode, message);
}

void setSpdlogLevel(const std::string_view level) {
    if (level == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else if (level == "error") {
        spdlog::set_level(spdlog::level::err);
    } else if (level == "warning") {
        spdlog::set_level(spdlog::level::warn);
    } else if (level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (level == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else {
        spdlog::set_level(spdlog::level::info);
    }
}

} // namespace

ApplicationEnvironment::ApplicationEnvironment(ConfigParams&& configParams)
    : m_configParams(std::move(configParams)) {
    spdlog::set_pattern("%^[%T.%e][%t][%n][%l]:%$ %v");
    setSpdlogLevel(m_configParams.logLevel);
    CRISP_LOGI("Current path: {}", std::filesystem::current_path().string());

    glfwSetErrorCallback(glfwErrorHandler);
    if (glfwInit() == GLFW_FALSE) {
        CRISP_LOGF("Could not initialize GLFW library!\n");
    }
}

ApplicationEnvironment::~ApplicationEnvironment() {
    glfwTerminate();
}

const std::filesystem::path& ApplicationEnvironment::getResourcesPath() const {
    return m_configParams.resourcesPath;
}

const std::filesystem::path& ApplicationEnvironment::getShaderSourceDirectory() const {
    return m_configParams.shaderSourcesPath;
}

const std::filesystem::path& ApplicationEnvironment::getOutputDirectory() const {
    return m_configParams.outputDir;
}

const ApplicationEnvironment::ConfigParams& ApplicationEnvironment::getConfigParams() const {
    return m_configParams;
}

std::vector<std::string> ApplicationEnvironment::getRequiredVulkanInstanceExtensions() {
    std::vector<std::string> extensions;
    uint32_t glfwExtensionCount{0};
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    extensions.reserve(glfwExtensionCount);
    for (unsigned int i = 0; i < glfwExtensionCount; i++) {
        extensions.emplace_back(glfwExtensions[i]); // NOLINT
    }

    return extensions;
}

Result<ApplicationEnvironment::CliParams> parse(const int32_t argc, char** argv) {
    ApplicationEnvironment::CliParams params{};
    CommandLineParser parser{};
    parser.addOption("config", params.configPath, /*isRequired=*/true);
    if (!parser.parse(argc, argv).isValid()) {
        return resultError("Failed to parse input arguments!");
    }

    return params;
}

Result<ApplicationEnvironment::ConfigParams> parseConfig(const std::filesystem::path& configPath) {
    const auto config = loadJsonFromFile(configPath).unwrap();
    ApplicationEnvironment::ConfigParams params{};
    params.logLevel = getIfExists<std::string>(config, "logLevel").value_or("info");
    params.resourcesPath = config["resourcesPath"].get<std::string>();
    params.shaderSourcesPath = config["shaderSourcesPath"].get<std::string>();
    params.outputDir = config["outputDir"].get<std::string>();
    params.imGuiFontPath = config["imguiFontPath"].get<std::string>();
    params.enableRayTracingExtension = getIfExists<bool>(config, "enableVulkanRayTracing").value_or(false);
    params.scene = config["scene"].get<std::string>();
    params.sceneArgs = config["sceneArgs"];
    return params;
}

} // namespace crisp
