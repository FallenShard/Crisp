#include <Crisp/ShaderUtils/ShaderType.hpp>
#include <Crisp/ShaderUtils/Test/TestShaderMap.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

const auto kShaderSourceDirectory = std::filesystem::path{"TestData"} / "CrispShaderCompilerTest";
const TestShaderMap kTestShaders{
    kShaderSourceDirectory / "reflection.comp",
};

TEST(ShaderCompilerTest, CompilesGlslToSpirvInProcess) {
    const auto spirv = kTestShaders.getSpirv("reflection.comp");

    ASSERT_FALSE(spirv.empty());
    EXPECT_EQ(spirv.front(), 0x07230203);
}

TEST(ShaderCompilerTest, InfersStageFromSlangFileName) {
    const auto stage = getShaderStageFromFilePath("GammaCorrect.frag.slang");

    ASSERT_TRUE(stage);
    EXPECT_EQ(*stage, VK_SHADER_STAGE_FRAGMENT_BIT);
}

} // namespace
} // namespace crisp
