#include <Crisp/Core/ApplicationEnvironment.hpp>

#include <fstream>
#include <string>
#include <vector>

#include <Crisp/Core/UniqueTemporaryFile.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(ApplicationEnvironmentTest, AppliesSelectedCommandLineOverridesAfterConfigFile) {
    UniqueTemporaryFile configFile("json", "application-environment-");
    {
        std::ofstream output(configFile.getPath(), std::ios::trunc);
        ASSERT_TRUE(output);
        output << R"json({
            "logLevel": "info",
            "resourcesPath": "resources-from-config",
            "shaderSourcesPath": "shaders-from-config",
            "outputDir": "output-from-config",
            "imguiFontPath": "font-from-config.ttf",
            "vulkan": {
                "forceValidationLayers": false,
                "enableRayTracing": false
            },
            "activeScene": "ocean",
            "scenes": {
                "atmosphere": {"rayMarchSteps": 32},
                "ocean": {"windSpeed": 12}
            }
        })json";
    }

    std::vector<std::string> arguments{
        "CrispMain",
        "--config_path",
        configFile.getPath().string(),
        "--scene=atmosphere",
        "--log_level",
        "debug",
        "--enable_ray_tracing",
        "true",
    };
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }

    auto result = parseConfig(static_cast<int32_t>(argv.size()), argv.data());
    ASSERT_TRUE(result.hasValue());
    const auto params = result.extract();

    EXPECT_EQ(params.logLevel, "debug");
    EXPECT_EQ(params.resourcesPath, "resources-from-config");
    EXPECT_EQ(params.shaderSourcesPath, "shaders-from-config");
    EXPECT_EQ(params.outputDir, "output-from-config");
    EXPECT_EQ(params.imGuiFontPath, std::filesystem::path("font-from-config.ttf"));
    EXPECT_FALSE(params.vulkan.forceValidationLayers);
    EXPECT_TRUE(params.vulkan.enableRayTracing);
    EXPECT_EQ(params.activeScene, "atmosphere");
    EXPECT_EQ(params.scenes.at("ocean"), nlohmann::json({{"windSpeed", 12}}));
    EXPECT_EQ(params.scenes.at("atmosphere"), nlohmann::json({{"rayMarchSteps", 32}}));
}

TEST(ApplicationEnvironmentTest, SupportsPartialConfigurationFiles) {
    UniqueTemporaryFile configFile("json", "application-environment-");
    {
        std::ofstream output(configFile.getPath(), std::ios::trunc);
        ASSERT_TRUE(output);
        output << R"json({"logLevel": "debug"})json";
    }

    auto result = parseConfig(configFile.getPath());
    ASSERT_TRUE(result.hasValue());
    const auto params = result.extract();

    EXPECT_EQ(params.activeScene, "ocean");
    EXPECT_EQ(params.logLevel, "debug");
    EXPECT_EQ(params.scenes, nlohmann::json({{"ocean", nlohmann::json::object()}}));
}

TEST(ApplicationEnvironmentTest, RejectsActiveSceneMissingFromCatalog) {
    UniqueTemporaryFile configFile("json", "application-environment-");
    {
        std::ofstream output(configFile.getPath(), std::ios::trunc);
        ASSERT_TRUE(output);
        output << R"json({"activeScene": "pbr", "scenes": {"ocean": {}}})json";
    }

    EXPECT_FALSE(parseConfig(configFile.getPath()).hasValue());
}

TEST(ApplicationEnvironmentTest, RejectsNonObjectSceneParameters) {
    UniqueTemporaryFile configFile("json", "application-environment-");
    {
        std::ofstream output(configFile.getPath(), std::ios::trunc);
        ASSERT_TRUE(output);
        output << R"json({"activeScene": "pbr", "scenes": {"pbr": true}})json";
    }

    EXPECT_FALSE(parseConfig(configFile.getPath()).hasValue());
}

} // namespace
} // namespace crisp
