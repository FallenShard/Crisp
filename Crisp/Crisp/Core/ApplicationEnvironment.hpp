#pragma once

#include <filesystem>

#include <Crisp/Io/JsonUtils.hpp>

namespace crisp {
class ApplicationEnvironment // NOLINT
{
public:
    struct Parameters {
        std::filesystem::path configPath;
        std::string scene{"ocean"};
        nlohmann::json sceneArgs;
        bool enableRayTracingExtension{false};
        std::string logLevel{"info"};
        std::filesystem::path outputDir{"D:/Projects/Crisp/Output"};
    };

    explicit ApplicationEnvironment(Parameters&& parameters);
    ~ApplicationEnvironment();

    // Returns the path to the folder where assets are stored
    const std::filesystem::path& getResourcesPath() const;

    // Returns the path to the folder where shader sources are stored
    // Useful for hot-reloading
    const std::filesystem::path& getShaderSourceDirectory() const;

    // Returns the names of the extensions that are required to successfully open a Vulkan window on this platform
    static std::vector<std::string> getRequiredVulkanInstanceExtensions();

    const Parameters& getParameters() const;

    const nlohmann::json& getConfig() const;

    const std::filesystem::path& getOutputDirectory() const;

private:
    std::filesystem::path m_resourcesPath;
    std::filesystem::path m_shaderSourcesPath;
    nlohmann::json m_config;

    Parameters m_arguments;
};

Result<ApplicationEnvironment::Parameters> parse(int32_t argc, char** argv);

} // namespace crisp
