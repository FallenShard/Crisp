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
    static std::filesystem::path getResourcesPath();

    // Returns the path to the folder where shader sources are stored
    // Useful for hot-reloading
    static std::filesystem::path getShaderSourceDirectory();

    // Returns the names of the extensions that are required to successfully open a Vulkan window on this platform
    static std::vector<std::string> getRequiredVulkanInstanceExtensions();

    static const Parameters& getParameters();

private:
    static std::filesystem::path ResourcesPath;
    static std::filesystem::path ShaderSourcesPath;

    static Parameters Arguments;
};

Result<ApplicationEnvironment::Parameters> parse(int argc, char** argv);

} // namespace crisp
