#pragma once

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

        static std::filesystem::path getResourcesPath();
        static std::filesystem::path getShaderSourcesPath();
        static std::vector<std::string> getRequiredVulkanExtensions();

    private:
        static std::filesystem::path ResourcesPath;
        static std::filesystem::path ShaderSourcesPath;
    };
}
