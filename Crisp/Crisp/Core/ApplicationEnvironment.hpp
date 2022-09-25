#pragma once

#include <Crisp/Core/CommandLineParser.hpp>

#include <filesystem>

namespace crisp
{
class ApplicationEnvironment
{
public:
    struct Parameters
    {
        std::string configPath{};
        uint32_t defaultSceneIndex{4};
        bool enableRayTracingExtension{false};
        std::string logLevel{"info"};
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

private:
    std::filesystem::path m_resourcesPath;
    std::filesystem::path m_shaderSourcesPath;

    Parameters m_arguments;
};

Result<ApplicationEnvironment::Parameters> parse(int argc, char** argv);

} // namespace crisp
