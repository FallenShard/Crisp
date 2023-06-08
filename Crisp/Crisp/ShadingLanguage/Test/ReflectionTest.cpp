#include <Crisp/ShadingLanguage/Reflection.hpp>

#include <gmock/gmock.h>

#include <spirv_reflect.h>

namespace crisp::sl::test
{
namespace
{
using ::testing::SizeIs;

const std::filesystem::path kAssetPath = "D:/Projects/Crisp/Crisp/Crisp/Shaders";

std::filesystem::path getShaderPath(const std::string& shaderName)
{
    return kAssetPath / (shaderName + ".glsl");
}

std::filesystem::path getSpirvShaderPath(const std::string& shaderName)
{
    const std::filesystem::path kSpvAssetPath = "D:/Projects/Crisp/Resources/Shaders";
    return kSpvAssetPath / (shaderName + ".spv");
}

TEST(ReflectionTest, VertexShader)
{
    const auto reflection = parseShaderUniformInputMetadata(getShaderPath("ocean-spectrum.comp")).unwrap();
    ASSERT_THAT(reflection.descriptorSetLayoutBindings, SizeIs(1));
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0], SizeIs(6));

    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][0].binding, 0);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][1].binding, 1);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][2].binding, 2);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][3].binding, 3);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][4].binding, 4);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][5].binding, 5);

    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][0].descriptorCount, 1);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][1].descriptorCount, 1);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][2].descriptorCount, 1);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][3].descriptorCount, 1);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][4].descriptorCount, 1);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][5].descriptorCount, 1);

    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][0].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][1].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][2].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][3].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][4].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    EXPECT_THAT(reflection.descriptorSetLayoutBindings[0][5].descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    ASSERT_THAT(reflection.pushConstants, SizeIs(1));
    EXPECT_THAT(reflection.pushConstants[0].offset, 0);
    EXPECT_THAT(reflection.pushConstants[0].size, 48);
    EXPECT_THAT(reflection.pushConstants[0].stageFlags, VK_SHADER_STAGE_COMPUTE_BIT);
}

TEST(ReflectionTest, SpirvReflect)
{
    SpvReflectShaderModule module;
    const auto spirv = readSpirvFile(getSpirvShaderPath("ocean-spectrum.comp")).unwrap();
    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &module);
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    uint32_t varCount = 0;
    result = spvReflectEnumerateInputVariables(&module, &varCount, nullptr);
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectInterfaceVariable*> inputVars(varCount);
    result = spvReflectEnumerateInputVariables(&module, &varCount, inputVars.data());
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    uint32_t descSetCount = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &descSetCount, nullptr);
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet*> descSets(descSetCount);
    result = spvReflectEnumerateDescriptorSets(&module, &descSetCount, descSets.data());
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    spv_reflect::ShaderModule shaderModule(spirv.size(), spirv.data());
    // shaderModule.
}

} // namespace
} // namespace crisp::sl::test