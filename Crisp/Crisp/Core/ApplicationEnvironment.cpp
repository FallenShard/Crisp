#include <Crisp/Core/ApplicationEnvironment.hpp>

#include <GLFW/glfw3.h>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/CommandLineParser.hpp>
#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("ApplicationEnvironment");

struct CliParams {
    std::filesystem::path configPath;
    std::optional<std::string> logLevel;
    std::optional<bool> enableRayTracingExtension;
    std::optional<std::string> scene;
};

template <typename T>
void applyOverride(T& target, std::optional<T>& overrideValue) {
    if (overrideValue) {
        target = std::move(*overrideValue);
    }
}

Result<CliParams> parseCommandLine(const int32_t argc, char** argv) {
    CliParams params{};
    CommandLineParser parser{};
    parser.addOption("config_path", params.configPath, /*isRequired=*/true);
    parser.addOption("enable_ray_tracing", params.enableRayTracingExtension);
    parser.addOption("log_level", params.logLevel);
    parser.addOption("scene", params.scene);

    if (!parser.parse(argc, argv).isValid()) {
        return resultError("Failed to parse command-line arguments");
    }
    return params;
}

bool isValidLogLevel(const std::string_view level) {
    return level == "critical" || level == "error" || level == "warning" || level == "warn" || level == "info" ||
           level == "debug" || level == "trace" || level == "off";
}

Result<> validateConfig(const ApplicationEnvironment::ConfigParams& params) {
    if (!isValidLogLevel(params.logLevel)) {
        return resultError("Invalid log level: {}", params.logLevel);
    }
    if (params.scene.empty()) {
        return resultError("The active scene must not be empty");
    }
    if (!params.sceneArgs.is_object()) {
        return resultError("sceneArgs must be a JSON object");
    }
    return {};
}

Result<> applyCliOverrides(ApplicationEnvironment::ConfigParams& params, CliParams& cliParams) {
    applyOverride(params.logLevel, cliParams.logLevel);
    applyOverride(params.enableRayTracingExtension, cliParams.enableRayTracingExtension);
    applyOverride(params.scene, cliParams.scene);

    return validateConfig(params);
}

void glfwErrorHandler(const int32_t errorCode, const char* message) {
    CRISP_LOGE("GLFW error code: {}. Message: {}", errorCode, message);
}

void setSpdlogLevel(const std::string_view level) {
    if (level == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else if (level == "error") {
        spdlog::set_level(spdlog::level::err);
    } else if (level == "warning" || level == "warn") {
        spdlog::set_level(spdlog::level::warn);
    } else if (level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (level == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else if (level == "off") {
        spdlog::set_level(spdlog::level::off);
    } else {
        spdlog::set_level(spdlog::level::info);
    }
}

} // namespace

ApplicationEnvironment::ApplicationEnvironment(ConfigParams&& configParams)
    : m_configParams(std::move(configParams)) {
    spdlog::set_pattern("%^[%T.%e][%t][%n][%l]:%$ %v");
    setSpdlogLevel(m_configParams.logLevel);
    CRISP_LOGI("Working directory: {}", std::filesystem::current_path().string());

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

Result<ApplicationEnvironment::ConfigParams> parseConfig(const std::filesystem::path& configPath) {
    CRISP_TRY(
        auto config,
        loadJsonFromFile(configPath),
        "Failed to load application configuration from {}",
        configPath.string());
    CRISP_CHECK(config.is_object(), "Application configuration must be a JSON object: {}", configPath.string());

    ApplicationEnvironment::ConfigParams params{};
    try {
        CRISP_PARSE_OPT(params.logLevel, config, "logLevel");
        CRISP_PARSE_OPT_TYPED(params.resourcesPath, config, "resourcesPath", std::string);
        CRISP_PARSE_OPT_TYPED(params.shaderSourcesPath, config, "shaderSourcesPath", std::string);
        CRISP_PARSE_OPT_TYPED(params.outputDir, config, "outputDir", std::string);
        if (config.contains("imguiFontPath") && !config["imguiFontPath"].is_null()) {
            params.imGuiFontPath = config["imguiFontPath"].get<std::string>();
        }

        CRISP_PARSE_OPT(params.enableValidationLayers, config, "enableValidationLayers");
        CRISP_PARSE_OPT(params.enableRayTracingExtension, config, "enableVulkanRayTracing");
        CRISP_PARSE_OPT_TYPED(params.scene, config, "scene", std::string);
        CRISP_PARSE_OPT(params.sceneArgs, config, "sceneArgs");
    } catch (const nlohmann::json::exception& exception) {
        return resultError("Invalid application configuration in {}: {}", configPath.string(), exception.what());
    }

    CRISP_TRY(validateConfig(params), "Invalid application configuration in {}", configPath.string());
    return params;
}

Result<ApplicationEnvironment::ConfigParams> parseConfig(const int32_t argc, char** argv) {
    CRISP_TRY(auto cliParams, parseCommandLine(argc, argv), "Could not read application command-line options");
    CRISP_TRY(auto configParams, parseConfig(cliParams.configPath), "Could not read application configuration");
    CRISP_TRY(applyCliOverrides(configParams, cliParams), "Invalid command-line configuration override");
    return configParams;
}

} // namespace crisp
