#pragma once

#include <CrispCore/CommandLineParser.hpp>

#include <filesystem>

namespace crisp
{
class ApplicationEnvironment
{
public:
    ApplicationEnvironment(int argc, char** argv);
    ~ApplicationEnvironment();

    // Returns the path to the folder where assets are stored
    static std::filesystem::path getResourcesPath();

    // Returns the path to the folder where shader sources are stored
    // Useful for hot-reloading
    static std::filesystem::path getShaderSourcesPath();

    // Returns the names of the extensions that are required to successfully open a Vulkan window on this platform
    static std::vector<std::string> getRequiredVulkanExtensions();

    static uint32_t getDefaultSceneIdx();

    static bool enableRayTracingExtension();

private:
    static std::filesystem::path ResourcesPath;
    static std::filesystem::path ShaderSourcesPath;

    static uint32_t DefaultSceneIdx;
    static bool EnableRayTracingExtension;
};
} // namespace crisp
