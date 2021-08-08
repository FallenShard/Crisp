#pragma once

#include <CrispCore/CommandLineParser.hpp>

#include <vector>
#include <string>
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

        // Returns the command line argument with the given name
        template <typename T>
        static T getArgument(const std::string_view name)
        {
            return CommandLineParser.get<T>(name);
        }

    private:
        static std::filesystem::path ResourcesPath;
        static std::filesystem::path ShaderSourcesPath;

        static CommandLineParser CommandLineParser;
    };
}
