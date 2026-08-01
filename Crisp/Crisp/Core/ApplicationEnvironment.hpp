#pragma once

#include <filesystem>

#include <Crisp/Io/JsonUtils.hpp>

namespace crisp {
class ApplicationEnvironment // NOLINT
{
public:
    struct ConfigParams {
        std::string logLevel{"info"};

        std::filesystem::path resourcesPath{"D:/Projects/Crisp/Resources"};
        std::filesystem::path shaderSourcesPath{"D:/Projects/Crisp/Crisp/Crisp/Shaders"};
        std::filesystem::path outputDir{"D:/Projects/Crisp/Output"};
        std::optional<std::filesystem::path> imGuiFontPath{std::nullopt};

        bool enableValidationLayers{true};
        bool enableRayTracingExtension{false};

        std::string scene{"ocean"};
        nlohmann::json sceneArgs{nlohmann::json::object()};
    };

    explicit ApplicationEnvironment(ConfigParams&& configParams);
    ~ApplicationEnvironment();

    // Returns the path to the folder where assets are stored.
    const std::filesystem::path& getResourcesPath() const;

    // Returns the path to the folder where shader sources are stored. Useful for hot-reloading.
    const std::filesystem::path& getShaderSourceDirectory() const;

    const std::filesystem::path& getOutputDirectory() const;

    const ConfigParams& getConfigParams() const;

    // Returns the names of the extensions that are required to successfully open a Vulkan window on this platform.
    static std::vector<std::string> getRequiredVulkanInstanceExtensions();

private:
    ConfigParams m_configParams;
};

Result<ApplicationEnvironment::ConfigParams> parseConfig(const std::filesystem::path& configPath);
Result<ApplicationEnvironment::ConfigParams> parseConfig(int32_t argc, char** argv);

} // namespace crisp
